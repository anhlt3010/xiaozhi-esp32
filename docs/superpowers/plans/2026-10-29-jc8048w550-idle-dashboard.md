# JC8048W550 Idle Dashboard Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the sample info cards on the Guition JC8048W550 idle screen with a real clock+weather card (today + 7 days) and a top-5 GitHub AI repo list that opens a detail card on tap.

**Architecture:** Everything lives in `main/boards/guition/jc8048w550/` (the board's `*.cc` files are globbed by `main/CMakeLists.txt`, so new files are picked up after a reconfigure). A pure data layer (types, helpers, JSON parsers) feeds a background `DashboardFetcher` task that publishes a `DashboardSnapshot`; `JC8048W550Display` applies it to an `IdleDashboard` LVGL view under the display lock. The existing right-panel switching (chat vs info) stays.

**Tech Stack:** ESP-IDF 6.1, C++23, LVGL 9.5, cJSON (`cjson_utils.h`), `Board::GetInstance().GetNetwork()->CreateHttp(...)`, Open-Meteo, GitHub search API, an HTTPS IP-geolocation service.

**Spec:** `docs/superpowers/specs/2026-10-29-jc8048w550-idle-dashboard-design.md` (mockup `2026-10-29-jc8048w550-idle-dashboard-mockup.html` beside it). Read both.

## Global Constraints

- Only touch `main/boards/guition/jc8048w550/` (plus the board's own block in `main/CMakeLists.txt`, `main/boards/guition/jc8048w550/config.json`). No core or other-board changes.
- Never block the main loop, the LVGL task or audio tasks with network calls; fetching runs in its own task; LVGL objects are touched only under `DisplayLockGuard` (or inside LVGL timers/event callbacks).
- Application mutations go through `Application::Schedule()`; no direct `SetDeviceState`.
- Validate network input; keep `cJSON` ownership via `CJsonUniquePtr` (`main/cjson_utils.h`).
- Text we render is ASCII: non-ASCII bytes in city/description/topics become `?`. Day names Mon..Sun, units °C.
- `LCD_PCLK_HZ` stays 16 MHz (12 MHz blanks the panel). Do not add continuous redraws; update labels/icons only when the value changed.
- Background `#18181C`, card `#24242c`, muted text `#8f8fa0`, star `#f5c542`, rank badges gold `#f5c542`/silver `#c0c0c8`/bronze `#cd7f32`/others `#3b82f6`.
- Refresh: weather every 30 min, repos every 60 min, retry after a failure in 5 min. Fetch only in `kDeviceStateIdle`, with the system clock synced (`tm_year >= 125`), and free internal RAM above a measured threshold. An in-flight request is allowed to finish (10 s timeout).
- Stale rule: a source is stale if its last attempt failed or its data is older than 2 refresh intervals; stale parts are dimmed with a small `offline` tag; never-fetched shows `no data`.
- Build: `python scripts/build.py guition/jc8048w550 --language vi-VN --wake-word wn9_hilili_tts` with ESP-IDF 6.1 (`IDF_TOOLS_PATH=N:\Espressif`, `N:\Espressif\frameworks\esp-idf-v6.1\export.ps1`). Flash: `idf.py -p COM5 app-flash`. `build.py` resets assets to default; the user's custom pack needs `CONFIG_FLASH_CUSTOM_ASSETS=y` and `CONFIG_CUSTOM_ASSETS_FILE="N:/xiaozhi-emoji/xiaozhi-emoji-v4-gstring/assets.bin"` in local `sdkconfig` (gitignored) before `idf.py build`.
- Commit steps below apply only if the user has asked for commits; otherwise say the work is uncommitted.
- A build is not hardware validation: report what still needs the device.

## Review Focus

Inputs and conditions the spec implies that no happy-path test covers (each has a test in its owning task):

1. A repo description/name/topic with non-ASCII bytes, emoji or 300+ chars: must render as `?`-substituted, truncated text, never overflow a row (Task 1, Task 5).
2. HTTP 403/429 (rate limit), non-200, empty body or malformed JSON: parser returns false, last data kept and marked offline, no leak or crash (Tasks 2, 3).
3. GitHub returns fewer than 5 items (or none): missing rows are hidden, no null access (Tasks 2, 5).
4. System clock not synced: clock card shows `--:--`, fetch does not start (Tasks 1, 5).
5. Detail overlay open when a conversation starts, and taps outside the overlay: overlay closes on leaving idle; tapping outside only closes it and never starts a chat (Task 6).
6. A fetch is in flight when the conversation starts: it finishes, and the result is applied later without touching LVGL off the lock (Task 3).

## File Structure

All under `main/boards/guition/jc8048w550/`:

- `dashboard_model.h/.cc`: data types, pure helpers, JSON parsers. No LVGL, no network.
- `dashboard_fetcher.h/.cc`: background task, schedule, gating, HTTP calls.
- `weather_icon.h/.cc`: LVGL-drawn weather icons.
- `idle_dashboard.h/.cc`: the idle view (summary card, repo list, detail overlay, clock).
- `jc8048w550_board.cc`: wiring (replace sample cards, tap, lifecycle).
- Modify `config.json`, `README.md`, and the board block of `main/CMakeLists.txt` (fonts).

---

### Task 1: Data model and pure helpers

**Files:**
- Create: `dashboard_model.h`, `dashboard_model.cc`

**Interfaces:**
- Produces (namespace `dashboard`):
  - `enum class WeatherIcon : uint8_t { kSunny, kPartlyCloudy, kCloudy, kFog, kRain, kThunder, kSnow };`
  - `constexpr WeatherIcon WeatherIconFromWmo(int wmo_code);` and `constexpr const char* WeatherTextFromWmo(int wmo_code);` (e.g. 0 Clear, 2 Partly cloudy, 3 Cloudy, 45 Fog, 61/63/65 Rain, 80..82 Showers, 95..99 Thunderstorm, 71..77/85/86 Snow; unknown → Cloudy / "Cloudy").
  - `constexpr const char* Weekday3(int year, int month, int day);` ("Mon".."Sun").
  - `std::string FormatStars(int stars);` (`950`→"950", `12400`→"12.4k", `1000`→"1.0k").
  - `std::string SanitizeAscii(std::string_view text, size_t max_chars);` (bytes >= 0x80 and control chars → `?`/space; result truncated to `max_chars`, no ellipsis).
  - `constexpr bool FetchAllowed(DeviceState state, bool time_synced, size_t free_internal_bytes, size_t min_free_internal_bytes);`
  - Structs: `ForecastDay {std::string weekday; WeatherIcon icon; int high_c; int low_c;}`, `Weather {std::string city; int temp_c; int high_c; int low_c; int humidity; WeatherIcon icon; std::string condition; std::array<ForecastDay,7> days;}`, `Repo {std::string full_name, description, language, updated; std::vector<std::string> topics; int stars; int forks;}`, `GeoLocation {double latitude, longitude; std::string city;}`, `template<class T> struct Source {T data{}; bool valid = false; bool last_failed = false; int64_t fetched_at_s = 0;}`, `struct Snapshot {Source<Weather> weather; Source<std::vector<Repo>> repos;}`.

- [ ] **Step 1: Write the failing tests** as `static_assert`s at the bottom of `dashboard_model.cc`: `Weekday3(2026,10,29)` equals `"Thu"` (compare with a constexpr `std::string_view`), `WeatherIconFromWmo(0)==kSunny`, `(2)==kPartlyCloudy`, `(63)==kRain`, `(96)==kThunder`, `(75)==kSnow`, `(45)==kFog`, `(1234)==kCloudy`; `FetchAllowed(kDeviceStateIdle,true,100000,60000)` true, and false for each of: state `kDeviceStateListening`, `time_synced=false`, `free=50000`.
- [ ] **Step 2: Run** `idf.py build` (after creating the header with declarations only) and see it fail to compile on the missing definitions/assertions.
- [ ] **Step 3: Implement** the constexpr helpers in the header (so `static_assert` can use them) and `FormatStars`/`SanitizeAscii` in the `.cc`. `Weekday3` uses the day-of-week algorithm on the civil date.
- [ ] **Step 4: Add a runtime self-check** for `FormatStars` and `SanitizeAscii` as `ESP_LOGI`-free `assert`-style cases in a `#ifdef DASHBOARD_SELF_TEST` function `RunModelSelfTest()` covering: `SanitizeAscii("caf\xC3\xA9 \xF0\x9F\x94\xA5", 20)=="caf?? ????"`-style (every non-ASCII byte becomes one `?`), a 300-char string truncated to `max_chars`, control char `\n` → space, `FormatStars(12400)=="12.4k"`, `FormatStars(999)=="999"`.
- [ ] **Step 5: Build** `python scripts/build.py guition/jc8048w550 --language vi-VN --wake-word wn9_hilili_tts`. Expected: build succeeds (all `static_assert`s hold).
- [ ] **Step 6: Commit** `feat(jc8048w550): dashboard data model and helpers`.

### Task 2: JSON parsers

**Files:**
- Modify: `dashboard_model.h/.cc`

**Interfaces:**
- Consumes: Task 1 types.
- Produces: `bool ParseGeoIp(const std::string& json, GeoLocation& out);`, `bool ParseWeather(const std::string& json, const std::string& city, Weather& out);`, `bool ParseRepos(const std::string& json, std::vector<Repo>& out);` (all use `CJsonUniquePtr`; return false and leave `out` untouched on any missing/mistyped required field; `ParseRepos` accepts 0–5 items, caps at 5, runs `SanitizeAscii` on name/description/language/topics with limits 48/200/24/24, at most 6 topics, `updated` = first 10 chars of `updated_at`).

- [ ] **Step 1: Capture real responses** with curl and save them under the scratchpad: Open-Meteo `https://api.open-meteo.com/v1/forecast?latitude=21.03&longitude=105.85&current=temperature_2m,relative_humidity_2m,weather_code&daily=weather_code,temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=8`; GitHub `https://api.github.com/search/repositories?q=topic:llm+created:>YYYY-MM-DD&sort=stars&order=desc&per_page=5` (header `User-Agent: xiaozhi-jc8048w550`, `Accept: application/vnd.github+json`, date = today minus 7 days); the geolocation endpoint `https://ipwho.is/` (swap for another keyless HTTPS service if it fails or lacks `latitude`, `longitude`, `city`). Expected: HTTP 200 and 5 repos. If `topic:llm` returns poor results, try `topic:ai`; record the query you chose in `dashboard_fetcher.cc` for Task 3. If the API rejects `OR` between topics, keep a single topic (spec).
- [ ] **Step 2: Write the failing check** in `RunModelSelfTest()` under `DASHBOARD_SELF_TEST`: embed trimmed copies of the three captured responses as `static const char[]` and assert: geo parses lat/lon/city; weather has 7 `days`, `days[0].weekday` computed from the response's `daily.time[1]`, `humidity` and `temp_c` set, `icon` from `weather_code`; repos has 5 entries with `stars > 0` and a `full_name` containing `/`. Add negative cases: `""`, `"{}"`, `"[1]"`, `{"items":[]}` → repos returns true with an empty vector; `{"message":"API rate limit exceeded"}` → false.
- [ ] **Step 3: Run** the self-test: define `DASHBOARD_SELF_TEST`, call `RunModelSelfTest()` once from the board constructor temporarily, build, flash, read the serial log. Expected: the new assertions fail (functions return false).
- [ ] **Step 4: Implement** the three parsers. Weather: today = `daily[0]`, next 7 days = `daily[1..7]`; icon/text from `WeatherIconFromWmo`/`WeatherTextFromWmo`; round temperatures to int.
- [ ] **Step 5: Run** the self-test again on the device; expected: all assertions pass (log line `dashboard self-test OK`). Remove the temporary call and the `DASHBOARD_SELF_TEST` define.
- [ ] **Step 6: Commit** `feat(jc8048w550): dashboard JSON parsers`.

### Task 3: Fetcher task

**Files:**
- Create: `dashboard_fetcher.h`, `dashboard_fetcher.cc`

**Interfaces:**
- Consumes: Task 1/2 types and parsers, `Application::GetInstance().GetDeviceState()`, `Board::GetInstance().GetNetwork()->CreateHttp(int)` (`Http::SetHeader/SetTimeout/Open/GetStatusCode/ReadAll/Close`).
- Produces:
  - `class DashboardFetcher { public: using Listener = std::function<void(const dashboard::Snapshot&)>; explicit DashboardFetcher(Listener on_update); void Start(); };`
  - Internals: `bool HttpGet(const std::string& url, std::string& body)` (User-Agent `xiaozhi-jc8048w550`, 10 s timeout, status must be 200, body capped at 64 KB), a task loop that wakes every 30 s, applies `FetchAllowed(...)` with a free-internal-RAM threshold constant `kMinFreeInternalBytes` (measure first, see Step 3), refreshes weather (geo-IP first, once per boot or after 3 failures, then Open-Meteo) and repos on their intervals, sets `last_failed`/`fetched_at_s` on each `Source`, and calls the listener with a copy of the whole snapshot.

- [ ] **Step 1: Write the failing check**: add a temporary `Start()` call from the board and a listener that logs one line per source, e.g. `dashboard: weather valid=%d city=%s temp=%d failed=%d`, `dashboard: repos n=%u top=%s stars=%d`. Expected before implementing: no log lines.
- [ ] **Step 2: Implement** the class. Task stack ≥ 8 KB, created with `xTaskCreate` (or the project's existing pattern for background tasks), priority below the audio tasks. Date for the GitHub query comes from `time(nullptr)` minus 7 days formatted `%Y-%m-%d` in UTC.
- [ ] **Step 3: Measure** free internal RAM (`heap_caps_get_free_size(MALLOC_CAP_INTERNAL)`) just before and during a fetch on the device and log both; set `kMinFreeInternalBytes` to a value leaving a safe margin above the lowest during-fetch value you saw (record the numbers in the commit message).
- [ ] **Step 4: Run on device** (flash firmware, connect Wi-Fi, wait idle). Expected logs: geo + weather fetched with the right city, 5 repos with `owner/name` and stars, repeated every interval; no `FATAL`/watchdog; the display and emoji animation keep running. Then unplug the router (or block the host) and expect `failed=1` with the old data kept. Start a conversation by touch while a fetch is running and expect the fetch to finish without a crash.
- [ ] **Step 5: Commit** `feat(jc8048w550): dashboard fetcher task`.

### Task 4: Weather icons

**Files:**
- Create: `weather_icon.h`, `weather_icon.cc`

**Interfaces:**
- Consumes: `dashboard::WeatherIcon`.
- Produces: `lv_obj_t* CreateWeatherIcon(lv_obj_t* parent, int32_t size);` and `void SetWeatherIcon(lv_obj_t* icon, dashboard::WeatherIcon kind);` (the object is a transparent square of `size` with child shapes redrawn by `SetWeatherIcon`; no-op when `kind` is unchanged; only LVGL primitives: circles/rounded rects/arcs/lines; colors: sun `#f5c542`, cloud `#c9ccd6`, rain drops `#4aa3ff`, lightning `#f5c542`, snow `#ffffff`).

- [ ] **Step 1: Write the failing check**: a temporary debug cycle in the board that creates one icon at 48 px and calls `SetWeatherIcon` through all 7 kinds, 1.5 s each. Expected before implementing: nothing visible.
- [ ] **Step 2: Implement** the two functions. The 7 kinds must be visually distinct at 24 px and 48 px.
- [ ] **Step 3: Flash and look**: each kind identifiable on the panel at both sizes; no flicker increase. Remove the debug cycle.
- [ ] **Step 4: Commit** `feat(jc8048w550): LVGL-drawn weather icons`.

### Task 5: Idle dashboard view

**Files:**
- Create: `idle_dashboard.h`, `idle_dashboard.cc`
- Modify: `config.json` (add the LVGL montserrat sizes you use as `CONFIG_LV_FONT_MONTSERRAT_nn=y`), board block of `main/CMakeLists.txt` (`BUILTIN_TEXT_FONT font_noto_sans_basic_20_4`, `BUILTIN_ICON_FONT font_material_symbols_20_4`)

**Interfaces:**
- Consumes: `dashboard::Snapshot`, `CreateWeatherIcon/SetWeatherIcon`, a `Fonts` struct.
- Produces:
  - `struct DashboardFonts { const lv_font_t* small; const lv_font_t* normal; const lv_font_t* temp; const lv_font_t* clock; const lv_font_t* icon; };`
  - `class IdleDashboard { public: IdleDashboard(lv_obj_t* content_parent, lv_obj_t* overlay_parent, int32_t content_width, const DashboardFonts& fonts); void Update(const dashboard::Snapshot& snapshot); void RefreshClock(); void HideDetail(); bool IsDetailOpen() const; };` — `Update`/`RefreshClock`/`HideDetail` are called with the LVGL lock held. `RefreshClock` reads `time()`/`localtime()`: year < 2025 → `--:--` and no date. `IdleDashboard` owns a 10-second `lv_timer` that calls `RefreshClock`.
  - Layout per the mockup: summary card (left: `HH:MM`, `Thu, 29 Oct 2026`, location icon + city; right: weather icon, temp, condition, `H33° L26° · 72%`; bottom row: 7 columns weekday/icon/`32°/26°`), a header row `GitHub · AI trending`, then 5 repo rows (rank badge, bold `owner/repo`, muted description, star icon + `FormatStars`). Rows are clickable (`LV_EVENT_CLICKED` opens the detail); the summary card and everything else is not clickable. Detail overlay: a full-size clickable scrim under `overlay_parent` plus a card (full name, description wrapped, language, stars, forks, topics, `updated`) and a close button; tapping the scrim or the button hides it, tapping the card does nothing.
  - Stale/no-data rules from Global Constraints.

- [ ] **Step 1: Write the failing check**: a temporary sample snapshot builder `MakeSampleSnapshot(int repo_count)` in the board plus a debug call that pushes snapshots with 5 repos, then 2 repos, then 0 repos with `weather.valid=false`, each held 5 s. Expected before implementing: nothing renders.
- [ ] **Step 2: Implement** `IdleDashboard`. Update labels/icons only when the text or icon actually changed (compare before setting) to avoid redraw churn. Rows beyond `repos.size()` are hidden. Row text uses `SanitizeAscii` limits so one line never overflows (`LV_LABEL_LONG_DOT`/clip; the name column has a fixed width).
- [ ] **Step 3: Flash and look**: with 5 repos the layout matches the mockup and fits 480 px without the right edge cutting anything; with 2 repos rows 3–5 vanish; with no weather the card shows `no data` for weather and the clock still ticks; a repo row with a 300-char description or non-ASCII characters stays on one line; tapping a row opens the detail and tapping outside closes it. Remove the debug calls.
- [ ] **Step 4: Commit** `feat(jc8048w550): idle dashboard view`.

### Task 6: Wire into the display and finish

**Files:**
- Modify: `jc8048w550_board.cc`, `README.md`, `.gitignore` is already updated

**Interfaces:**
- Consumes: `IdleDashboard`, `DashboardFetcher`.
- Produces: `JC8048W550Display` owns `std::unique_ptr<IdleDashboard> dashboard_` created in `BuildRightPanel()` in place of the three `AddInfoCard` sample cards (delete `AddInfoCard`/`ColorCard` and the sample text); `void ApplyDashboardSnapshot(dashboard::Snapshot snapshot)` (public; takes `DisplayLockGuard`, calls `dashboard_->Update`); `SetStatus` calls `dashboard_->HideDetail()` when `talking_` becomes true; the board constructor creates a `DashboardFetcher` whose listener calls `display_->ApplyDashboardSnapshot(...)` and starts it after the display and Wi-Fi are ready.

- [ ] **Step 1: Write the failing checks** (manual, device): (a) with the old sample cards removed, the idle screen is empty until data arrives; (b) tapping a repo row opens detail; (c) with detail open, tap outside → closes and does NOT start a chat; (d) with detail open, start a conversation (tap a non-row area is blocked by the overlay, so use the wake word or MCP/another trigger, or close and re-tap) → overlay is gone when the chat appears; (e) tap on the summary card or empty area → chat starts as before.
- [ ] **Step 2: Implement** the wiring. Keep `OnTap` and the screen/container clickable list unchanged; repo rows and the overlay scrim are the only new clickable objects, so everything else still falls through to `container_`.
- [ ] **Step 3: README**: add a "Idle dashboard" section to `main/boards/guition/jc8048w550/README.md` naming the three services, that the device's IP address goes to the geolocation service and the approximate location to Open-Meteo, the refresh intervals, and that data is not stored.
- [ ] **Step 4: Verify**: build (`build.py`, then re-apply the custom-assets lines and `idf.py build` if the custom pack is wanted), flash `idf.py -p COM5 app-flash`, walk through checks (a)–(e) plus: unplug Wi-Fi → clock still runs, weather/list show `offline`; long idle → data refreshes at the intervals; emoji animation flicker is not worse than before; serial log shows no watchdog, and free internal RAM stays above the threshold. Run `python -m unittest discover -s scripts/tests -v` and report any failures with whether they pre-exist (they did on this Windows machine: 1 failure, 12 errors in `test_build`, `test_ci_selection`, `test_ogg_demuxer`).
- [ ] **Step 5: Format** touched C/C++ files with `clang-format -i` (install or use the IDF `esp-clang` tool if `clang-format` is not on PATH) and `clang-format --dry-run -Werror <files>`.
- [ ] **Step 6: Report** what was tested and what still needs the device; commit `feat(jc8048w550): real clock, weather and GitHub AI trending on the idle screen` only if the user has asked for commits.
