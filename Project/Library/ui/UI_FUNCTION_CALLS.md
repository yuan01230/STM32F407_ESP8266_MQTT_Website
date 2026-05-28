> 文档导航
> - [UI 目录说明](./README.md)
> - [UI 函数调用关系](./UI_FUNCTION_CALLS.md)
> - [UI API 手册](./UI_API_MANUAL.md)
# UI 鍑芥暟璋冪敤鍏崇郴涓庡姛鑳借鏄?
鏈枃妗ｆ弿杩?`Library/ui` 鐩綍涓嬪悇鏂囦欢涓嚱鏁扮殑璋冪敤鍏崇郴锛屼互鍙婃瘡涓嚱鏁扮殑涓昏鑱岃矗銆?
## 1. 鎬讳綋璋冪敤鍏崇郴锛堜粠 main 鍒伴〉闈㈠嚱鏁帮級

`Core/Src/main.c` 鍦ㄤ富娴佺▼涓皟鐢細

1. `UI_Init()`
2. `UI_SetSdMounted(...)`
3. `UI_FontSystemInit()`
4. `UI_ShowMainPage()`
5. 涓诲惊鐜弽澶嶈皟鐢細
   1. `UI_Tick()`
   2. `UI_HandleKey(key)`

## 2. 璋冨害灞?`ui_app.c`

### 2.1 鍒濆鍖?鍛ㄦ湡鍏ュ彛

- `UI_Init()`
  - 鍒濆鍖?UI 椤甸潰鐘舵€併€佽鏃跺熀鍑嗐€?
- `UI_SetSdMounted(uint8_t mounted)`
  - 鍚屾 SD 鎸傝浇鐘舵€佸埌 UI 鍏ㄥ眬鍙橀噺銆?
- `UI_FontSystemInit()`
  - 璋冪敤 `UI_Font_ImportSystemInit(g_sd_mounted)` 瀹屾垚鍚姩闃舵瀛楀簱鍙敤鎬ф鏌ヤ笌蹇呰瀵煎叆銆?
- `UI_ShowMainPage()`
  - 璋冪敤 `UI_CountRootFiles()` 鏇存柊鏂囦欢鎬绘暟
  - 璋冪敤 `UI_MainPage_Show()` 缁樺埗涓婚〉闈?  - 璋冪敤 `UI_MainPage_UpdateRuntime(...)` 鍒锋柊杩愯鏃堕棿

- `UI_Tick()`
  - 闈炰富椤甸潰瓒呮椂锛?0s锛夎嚜鍔ㄨ繑鍥炰富椤甸潰
  - 姣忕鍒锋柊锛?    - 涓婚〉闈細`UI_MainPage_UpdateRuntime(...)`
    - 鍒楄〃椤碉細`UI_ListPage_Show()`

### 2.2 鎸夐敭璺敱鍏ュ彛

- `UI_HandleKey(KeyName_t key)`
  - `KEY0`
    - 鍒楄〃椤?鏌ョ湅椤?-> 鍥炰富椤甸潰
    - 涓婚〉闈?-> `UI_LoadRootFileList()` + `UI_EnterPage(UI_PAGE_FILE_LIST)` + `UI_ListPage_Show()`
  - `KEY1`
    - 涓婚〉闈?-> 鍒囨崲棰滆壊妯″紡骞堕噸缁樹富椤甸潰
    - 鍒楄〃椤?-> `UI_List_SelectDown()` + `UI_ListPage_Show()`
  - `KEY2`
    - 涓婚〉闈?-> `UI_CreateTestFileAndShow()` -> `UI_ShowMainPage()`
    - 鍒楄〃椤?-> `UI_View_OpenSelected()`
    - 鏌ョ湅椤?-> 鍥炲垪琛ㄩ〉骞?`UI_ListPage_Show()`
  - `KEY_UP`
    - 涓婚〉闈?-> `UI_Font_RunImportFromMain()` -> 閲嶇粯涓婚〉闈?    - 鍒楄〃椤?-> `UI_List_SelectUp()` + `UI_ListPage_Show()`

## 3. 鍏叡灞?`ui_common.c`

### 3.1 椤甸潰涓庢椂闂?
- `UI_EnterPage(UiPage_t page)`
  - 鍒囨崲椤甸潰骞跺埛鏂版棤鎿嶄綔璁℃椂銆?  - 杩涘叆鍒楄〃椤垫椂缃綅 `g_file_list_need_full_redraw`銆?
- `UI_GetCurrentSeconds()`
  - 杩斿洖褰撳墠绯荤粺绉掓暟锛坄HAL_GetTick()/1000`锛夈€?
- `UI_GetRemainSeconds()`
  - 杩斿洖鑷姩鍥炰富椤甸潰鍓╀綑绉掓暟銆?
### 3.2 鏂囦欢鍒楄〃搴曞眰

- `UI_CountRootFiles()`
  - 缁熻 SD 鏍圭洰褰曟櫘閫氭枃浠舵暟锛堜笉鍚洰褰曪級銆?
- `UI_LoadRootFileList()`
  - 鍔犺浇 SD 鏍圭洰褰曟枃浠跺埌 `g_file_entries[]`銆?  - 濉厖 `name/ext/size` 骞剁淮鎶?`g_file_count`銆侀€変腑绱㈠紩鍚堟硶鎬с€?
### 3.3 鏂囨湰缂栫爜涓庢樉绀哄簳灞?
- `UI_IsValidUtf8(...)`
  - 鍒ゆ柇缂撳啿鍖烘槸鍚︽湁鏁?UTF-8 鏂囨湰銆?
- `UI_Utf8DecodeCodepoint(...)`
  - 瑙ｇ爜鍗曚釜 UTF-8 鐮佺偣銆?
- `UI_Utf8ToGb2312(...)`
  - UTF-8 瀛楄妭娴佽浆鎹负 GB2312 瀛楄妭娴併€?
- `UI_TextUtf8ToGb2312(...)`
  - UTF-8 瀛楃涓蹭究鎹疯浆鎹㈡帴鍙ｃ€?
- `UI_LCD_ShowUtf8(...)`
  - 鐩存帴浼?UTF-8 瀛楃涓插埌 LCD锛屽唴閮ㄨ嚜鍔ㄨ浆 GB2312 鍚庤皟鐢?`LCD_ShowTextMixed(...)`銆?
## 4. 涓婚〉闈?`ui_main.c`

- `UI_MainPage_Show()`
  - 缁樺埗涓婚〉闈㈡枃瀛楁紨绀恒€佹寜閿彁绀恒€佺姸鎬佸尯锛圫D/Font/Files锛夈€?  - 浣跨敤棰滆壊妯″紡 `g_test_fg_mode` 鎺у埗鏂囨湰棰滆壊銆?
- `UI_MainPage_UpdateRuntime(uint32_t seconds)`
  - 灞€閮ㄥ埛鏂颁富椤甸潰杩愯鏃堕棿鍖哄煙锛岄伩鍏嶆暣椤甸噸缁橀棯鐑併€?
## 5. 鍒楄〃椤?`ui_list.c`

- `UI_ListPage_Show()`
  - 鍒楄〃椤垫樉绀哄叆鍙ｃ€?  - 鍐呴儴鎸夌姸鎬侀€夋嫨鈥滄暣椤甸噸缁樷€濇垨鈥滃眬閮ㄥ埛鏂帮紙鏃ч€変腑琛?鏂伴€変腑琛?鍊掕鏃讹級鈥濄€?
- `UI_List_SelectDown()`
  - 閫変腑椤逛笅绉伙紙寰幆锛夛紝骞剁淮鎶ゅ垎椤电獥鍙ｉ《閮ㄧ储寮曘€?
- `UI_List_SelectUp()`
  - 閫変腑椤逛笂绉伙紙寰幆锛夛紝骞剁淮鎶ゅ垎椤电獥鍙ｉ《閮ㄧ储寮曘€?
- `UI_List_ShowRow(...)`锛坰tatic锛?  - 缁樺埗鍗曡鏂囦欢淇℃伅锛堢澶淬€佹枃浠跺悕銆佹墿灞曞悕銆佸ぇ灏忥級銆?
## 6. 鏌ョ湅椤?`ui_view.c`

- `UI_View_OpenSelected()`
  - 鎵撳紑褰撳墠閫変腑鏂囦欢锛屽垏鎹㈠埌鏌ョ湅椤靛苟璋冪敤 `UI_View_Show(...)`銆?
- `UI_View_Show(uint16_t index)`
  - 璇诲彇鏂囦欢鍓?`FILE_VIEW_READ_MAX` 瀛楄妭骞跺垎绫绘樉绀猴細
    - GB2312/ASCII 鐩存帴鏄剧ず
    - UTF-8 杞?GB2312 鍚庢樉绀?    - 闈炴枃鏈寜鍗佸叚杩涘埗鎽樿鏄剧ず

## 7. 瀛楀簱瀵煎叆椤?`ui_font_import.c`

- `UI_Font_ImportSystemInit(uint8_t sd_ready)`
  - 鍚姩闃舵瀛楀簱鍒濆鍖栵細鑻ュ閮?Flash 鏃犲彲鐢ㄥ瓧搴撲笖 SD 鍙敤锛屽垯瀵煎叆銆?
- `UI_Font_RunImportFromMain()`
  - 涓婚〉闈㈡墜鍔ㄨЕ鍙戦噸瀵煎叆銆?
- `UI_Font_DoImport()`锛坰tatic锛?  - 缁熶竴瀵煎叆鎵ц娴佺▼锛堝惎鍔ㄥ鍏?鎵嬪姩瀵煎叆鍏辩敤锛夈€?
- `UI_Font_ProgressCb(...)`锛坰tatic锛?  - 瀵煎叆杩涘害鍥炶皟锛岃绠楃櫨鍒嗘瘮骞舵洿鏂拌繘搴﹂〉闈€?
- `UI_Font_ShowProgress(...)`锛坰tatic锛?  - 瀵煎叆椤甸潰鏄剧ず涓庡眬閮ㄥ埛鏂帮紙鐘舵€併€佺櫨鍒嗘瘮銆佸瓧鑺傝鏁般€佽繘搴︽潯锛夈€?
## 8. 鍒涘缓鏂囦欢椤?`ui_create_file.c`

- `UI_CreateTestFileAndShow()`
  - 鍦?SD 鏍圭洰褰曞垱寤?`娴嬭瘯涓撶敤.txt`锛?    - 鍐欏叆 RTC 鏃堕棿
    - 鍐欏叆涓枃琛岋紙UTF-8 杞?GB2312锛?    - 鍐欏叆鑻辨枃琛?  - 瀹屾垚鍚庢樉绀虹粨鏋滈〉銆?
- `UI_Create_ShowPreview(...)`锛坰tatic锛?  - 鏄剧ず鍒涘缓缁撴灉涓庡啓鍏ラ瑙堬紝3 绉掑悗杩斿洖涓婂眰娴佺▼銆?
## 9. 鍏抽敭鍏变韩鍙橀噺锛堟潵鑷?`ui_common.c`锛?
- `g_ui_page`锛氬綋鍓嶉〉闈㈢姸鎬?- `g_ui_last_action_tick`锛氭渶鍚庢寜閿椂鍒?- `g_file_entries[]`锛氭枃浠剁紦瀛?- `g_file_count`锛氭枃浠舵€绘暟
- `g_selected_index`锛氬綋鍓嶉€変腑绱㈠紩
- `g_list_top_index`锛氬垪琛ㄩ〉闈㈢獥鍙ｉ《閮ㄧ储寮?- `g_file_list_need_full_redraw`锛氬垪琛ㄩ〉鏄惁寮哄埗鏁撮〉閲嶇粯
- `g_test_fg_mode`锛氫富椤甸潰鏂囨湰棰滆壊妯″紡
- `g_sd_mounted`锛歋D 鎸傝浇鐘舵€?

