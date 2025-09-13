#include <QuickDraw.h>
#include <Fonts.h>
#include <Windows.h>
#include <Menus.h>
#include <Events.h>
#include <TextEdit.h>
#include <GestaltEqu.h>
#include <OSUtils.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "dlcalc.h"

#define APP_SLEEP 5
#define N_KEYS 32
#define N_ROWS 6
#define N_COLS 6

enum {
	kDLMacRedButtonColor,
	kDLMacBlackButtonColor,
	kDLMacGrayButtonColor,
	kDLMacEmptyColor,
	kDLShouldNotDraw = -1
};

enum {
	kIconGT = 128,
	kIconE,
	kIconM,
	kIconMinus,
	kIconApos,
	kIconDot,
	kIconStub
};

enum {
	kDLMacButtonSizeS,
	kDLMacButtonSizeL,
	kDLMacButtonSizeXL,
	kDLMacButtonSizeInvalid = -1
};

enum {
	kDLMacNotTriggered,
	kDLMacTriggeredByMouse,
	kDLMacTriggeredByKeyboard
};

extern void tRenderLCD();

const RGBColor DL_MAC_FOREGROUNDS[] = {{0xFFFF,0xFFFF,0xFFFF},{0xFFFF,0xFFFF,0xFFFF},{0xFFFF,0xFFFF,0xFFFF},{0xFFFF,0xFFFF,0xFFFF}};
const RGBColor DL_MAC_BACKGROUNDS[] = {{141<<8,29<<8,80<<8}, {0,0,0}, {147<<8, 147<<8, 147<<8}, {231<<8, 231<<8, 231<<8}};
const RGBColor DL_LCD_BACKGROUND = {220<<8,247<<8,219<<8};
const RGBColor DL_LCD_FOREGROUND = {0,0,0};
const static int DL_MAC_KEYPAD_GRID_X[] = {5,  50, 85, 120, 168, 204};
const static int DL_MAC_KEYPAD_GRID_Y[] = {38, 66, 93, 131, 168, 206};
const static int DL_MAC_KEY_COLOR_CONFIG[] = {1,1,
											1,1,1,1,1,1,
											1,2,2,2,1,1,
											1,2,2,2,1,1,
											0,2,2,2,1,1,
											0,2,2,2,-1,1};
const static int DL_MAC_KEY_SIZE_CONFIG[] = {0,0,
											 0,0,0,0,0,0,
											 1,1,1,1,1,1,
											 1,1,1,1,1,1,
											 1,1,1,1,2,1,
											 1,1,1,1,-1,1};
const static int DL_MAC_BUTTON_WIDTH[] = {33,33,33,0};
const static int DL_MAC_BUTTON_HEIGHT[] = {18,30,68,0};
const static int DL_MAC_LABEL_OFFSET_X[] = {7,5,
											6,5,5,5,5,8,
											1,10,10,10,10,2,
											5,10,10,10,13,13,
											8,10,10,10,13,13,
											8,10,7,13,13,13};
const static int DL_MAC_LABEL_OFFSET_Y[] = {13,13,
											13,13,13,13,13,13,
											20,20,20,20,20,20,
											20,20,20,20,20,20,
											20,20,20,20,40,20,
											20,20,20,20,20,20};
const static unsigned char *DL_MAC_LABELS[] = {"\pEX","\pOFF",
									  "\pMU","\pMC","\pMR","\pM-","\pM+","\pGT",
									  "\pBksp","\p7","\p8","\p9","\p%","\pSqrt",
									  "\p+/-","\p4","\p5","\p6","\px","\p/",
									  "\pCE","\p1","\p2","\p3","\p+","\p-",
									  "\pAC","\p0","\p00","\p.","\p?","\p="};
const static char DL_MAC_ACCEL[] = {'q','w',
									'e','r','t','y','u','i',
									'a','7','8','9','s','d',
									'f','4','5','6','*','/',
									'c','1','2','3','+','-',
									'z','0',' ','.','\0','\r'};
									
const static char DL_MAC_FIX_MAP[] = {DF_ADD2,DF_4,DF_3,DF_2,DF_0,DF_F};
const static char DL_MAC_ROUND_MAP[] = {DM_CEIL,DM_ROUND,DM_FLOOR};
const static char DL_MAC_FIX_MAP_R[] = {1,2,3,4,5,6};
const static char DL_MAC_ROUND_MAP_R[] = {1,2,3};
void tCopyToClipboard(EventRecord *evt);

WindowPtr win;
Handle mbar;
int gShouldEnd = 0;
int gKeyHighlight = -1;
int gKeyTrigger = kDLMacNotTriggered;
int gTargetKc;
int gFreezeScreen = 0;
int gDebug = 0;

unsigned long time_app_start;
unsigned long time_now;

unsigned char gBlinkingCycle = 0;
DLCalc *gCalc;
static Str255 errmsg_buffer;
char gLCDBits[30][30] = {0};
static Rect gLCDRect = {0,0,30,240};
static BitMap gLCDBmp = {(char *)gLCDBits, 30, {0,0,30,240}};
static Rect gDebugRect = {31,1,64,165};

short gChicago = 0;

enum {
	kDialogDefinition = 128,
	kDialogCQD = 128,
	kAlertAbout = 129,
	kAlertDLFail = 130,
	kMyWindow = 128,
	kMBar = 128,
	kAppleMenu = 128,
	kFixMenu = 129,
	kRoundMenu = 130,
	kFileMenu = 131
};

void InitToolbox(void);
void CheckSysRequirements();

void InitToolbox()
{
	InitGraf((Ptr) &qd.thePort);
	InitFonts();
	InitWindows();
	InitMenus();
	FlushEvents(everyEvent,0);
	TEInit();
	InitDialogs(0L);
	InitCursor();
}

void CheckSysRequirements()
{
	// We want a Color QD.
	long response;
	OSErr gestResult = Gestalt(gestaltQuickdrawVersion,&response);
	if (gestResult != noErr) {
		NoteAlert(kDialogCQD,NULL);
		ExitToShell();
	}
}

void CreateWindow() {
	win = GetNewCWindow(kMyWindow,NULL,(WindowPtr)-1);
	if (!win) {
		SysBeep(30);
		ExitToShell();
	}
	GetFNum("\pChicago", &gChicago);
}

void tMenu(long result) {
	Str255 itemName;
	short major = HiWord(result);
	short minor = LoWord(result);
	if (major == kAppleMenu) {
		if (minor == 1) {
			int response = Alert(kAlertAbout,NULL);
			if (response == 3) {
				// Show OSS Notice
				short ret;
				DialogPtr oss = GetNewDialog(128,NULL,(WindowPtr)-1);
				ModalDialog(NULL,&ret);
				DisposeDialog(oss);
			}
		} else {
			GetMenuItemText(GetMenuHandle(kAppleMenu),minor,itemName);
			OpenDeskAcc(itemName);
		}
	} else if (major == kFileMenu) {
		if (minor == 1)
			tCopyToClipboard(NULL);
		else if (minor == 2)
			ExitToShell();
	} else if (major == kRoundMenu) {
		gCalc->roundMode = DL_MAC_ROUND_MAP[minor-1];
	} else if (major == kFixMenu) {
		gCalc->fixMode = DL_MAC_FIX_MAP[minor-1];
	}
	if (gDebug) {
		SetPort(win);
		InvalRect(&gDebugRect);
	}
	HiliteMenu(0);
}

int tFindButton(EventRecord *evt) {
	int x,y,i,sidx,ox,oy,right,bottom;
	Point p = evt->where;
	GlobalToLocal(&p);
	for (y = 0; y < N_ROWS; y++) {
		for (x = 0; x < N_COLS; x++) {
			i = y * N_COLS + x - 4; 
			if (i < 0)
				continue;
			sidx = DL_MAC_KEY_SIZE_CONFIG[i];
			if (sidx < 0) continue;
			ox = DL_MAC_KEYPAD_GRID_X[x];
			oy = DL_MAC_KEYPAD_GRID_Y[y];
			right = ox + DL_MAC_BUTTON_WIDTH[sidx];
			bottom = oy + DL_MAC_BUTTON_HEIGHT[sidx];
			if (p.h >= ox && p.h < right && p.v >= oy && p.v < bottom)
				return i;
		}
	}
	return -1;
}

void tTranslateKeyCode(int i, int *x, int *y) {
	*y = (i + 4) / N_COLS;
	*x = (i + 4) % N_COLS;
}

int tScanCodeToKeyCode(int scanCode) {
	const static int map[] = {DK_EX,DK_OFF,
							  DK_MU,DK_MC,DK_MR,DK_M_MINUS,DK_M_PLUS,DK_GT,
							  DK_BS,DK_7,DK_8,DK_9,DK_PERC,DK_SQRT,
							  DK_SIGN,DK_4,DK_5,DK_6,DK_MULT,DK_DIV,
							  DK_CE,DK_1,DK_2,DK_3,DK_PLUS,DK_MINUS,
							  DK_AC,DK_0,DK_00,DK_PERIOD,-1,DK_EQU};
	return map[scanCode];
}

int tGetButtonRect(int buttonId, Rect *r) {
	int sidx,x,y,ox,oy,right,bottom;
	sidx = DL_MAC_KEY_SIZE_CONFIG[buttonId];
	if (sidx < 0) return 1;
	tTranslateKeyCode(buttonId,&x,&y);
	ox = DL_MAC_KEYPAD_GRID_X[x];
	oy = DL_MAC_KEYPAD_GRID_Y[y];
	right = ox + DL_MAC_BUTTON_WIDTH[sidx];
	bottom = oy + DL_MAC_BUTTON_HEIGHT[sidx];
	SetRect(r,ox,oy,right,bottom);
	return 0;
}

void tTryKeypad(EventRecord *evt) {
	int keyCode;
	
	keyCode = tFindButton(evt);
	if (keyCode >= 0) {
		Rect r, old_r; int result;
		
		if (gKeyHighlight >= 0) {
			// Unhighlight old key
			result = tGetButtonRect(gKeyHighlight,&old_r);
			if (!result) {
				gKeyHighlight = -1;
				SetPort(win);
				InvalRect(&old_r);
			}
		}
		
		gKeyHighlight = keyCode;
		gKeyTrigger = kDLMacTriggeredByMouse;
		result = tGetButtonRect(keyCode,&r);
		if (result) {
			SysBeep(30);
			return;
		}
		
		DLOPPushButton(gCalc,tScanCodeToKeyCode(gKeyHighlight));
		DLLCDRender(gCalc,0);
		SetPort(win);
		// PaintRect(&r);
		InvalRect(&r);
		InvalRect(&gLCDRect);
		if (gDebug) {
			InvalRect(&gDebugRect);
		}
	} else {
		SysBeep(30);
	}
}

void tAdjustMenu() {
	MenuHandle fixMenu = GetMenuHandle(129);
	MenuHandle roundMenu = GetMenuHandle(130);
	MenuHandle fileMenu = GetMenuHandle(131);
	int i;
	if (!gCalc || !gCalc->power)
		DisableItem(fileMenu,1);
	else
		EnableItem(fileMenu,1);
	for (i=1; i<=3; i++) {
		SetItemMark(roundMenu,i,noMark);
	}
	for (i=1; i<=6; i++) {
		SetItemMark(fixMenu,i,noMark);
	}
	SetItemMark(roundMenu,DL_MAC_ROUND_MAP_R[gCalc->roundMode],checkMark);
	SetItemMark(fixMenu,DL_MAC_FIX_MAP_R[gCalc->fixMode],checkMark);
}

void tMouseDown(EventRecord *evt) {
	int part;
	WindowPtr theWindow;
	part = FindWindow(evt->where, &theWindow);
	switch (part) {
		case inSysWindow:
			SystemClick(evt,theWindow);
			break;
		case inContent: case inGrow:
			if (FrontWindow() != theWindow)	
				SelectWindow(theWindow);
			else {
				tTryKeypad(evt);
			}
			break;
		case inDrag:
			DragWindow(theWindow, evt->where, &((**GetGrayRgn()).rgnBBox));
			break;
		case inGoAway:
			if (TrackGoAway(theWindow, evt->where)) {
				ExitToShell();
			}
			break;
		case inMenuBar:
			tAdjustMenu();
			tMenu(MenuSelect(evt->where));
			break;
	}
}


void tMouseUp(EventRecord *evt) {
	if (gKeyHighlight >= 0 && gKeyTrigger == kDLMacTriggeredByMouse) {
		Rect r;
		int result;
		// SysBeep(0);// TODO: Change this to Key event later.
		result = tGetButtonRect(gKeyHighlight,&r);
		if (result) return;
		SetPort(win);
		// PaintRect(&r);
		gKeyHighlight = -1;
		gKeyTrigger = kDLMacNotTriggered;
		InvalRect(&r);
	}
}

void tKeyUp(EventRecord *evt) {
	if (gKeyHighlight >= 0 && gKeyTrigger == kDLMacTriggeredByKeyboard) {
		Rect r;
		int result;
		result = tGetButtonRect(gKeyHighlight,&r);
		if (result) return;
		SetPort(win);
		gKeyHighlight = -1;
		gKeyTrigger = kDLMacNotTriggered;
		gTargetKc = -1;
		InvalRect(&r);
	}
}

void tCopyToClipboard(EventRecord *evt) {
	char buf[255];
	ZeroScrap();
	DLFormatDLNumber(&gCalc->buffer,buf);
	PutScrap(strlen(buf),'TEXT',buf);
}

void tKeyDown(EventRecord *evt) {
	long msg = evt->message;
	char chr = msg & 0xFF;
	char kc = (msg >> 8) & 0xFF;
	int i, result;
	Rect r, old_r;
	
	if (evt->modifiers & cmdKey) {
		switch (chr) {
			case 'q':
				HiliteMenu(kFileMenu);
				ExitToShell();
				break;
			case 'c':
				if (gCalc->power) {
					HiliteMenu(kFileMenu);
					tCopyToClipboard(evt);
					HiliteMenu(0);
				} else SysBeep(30);
				break;
			case 'd':
				if (evt->modifiers & shiftKey) {
					gDebug = !gDebug;
					SetPort(win);
					InvalRect(&win->portRect);
				} else {
					SysBeep(30);
				}
				break;
			default:
				SysBeep(30);
				break;
		}	
		return;
	}
	
#ifdef DL_DEBUG
	KeyMap km;
	Str255 buf;
	const static Rect dbgr = {0,0,60,240};
	char *ptr = (char *)buf;
	TextFont(4);
	GetKeys(km);
	SetPort(win);
	MoveTo(0,16);
	TextSize(9);
	BackColor(blackColor);
	ForeColor(whiteColor);
	EraseRect(&dbgr);
	ptr[0] = sprintf(ptr+1,"KC=%02x; KM=", (unsigned char)kc);
	DrawString(buf);
	MoveTo(0,26);
	ptr[0] = sprintf(ptr+1,"%08lx %08lx", (unsigned long)km[0], (unsigned long)km[1]);
	DrawString(buf);
	MoveTo(0,36);
	ptr[0] = sprintf(ptr+1,"%08lx %08lx",(unsigned long)km[2],(unsigned long)km[3]);
	DrawString(buf);
#endif
	
	for (i = 0; i < N_KEYS; i++) {
		if (chr == DL_MAC_ACCEL[i] || kc == 0x4C) {
			if (kc == 0x4C) {
				i = 31;
			}
			result = tGetButtonRect(i,&r);
			if (result) break;
			if (gKeyHighlight >= 0) {
				// Unhighlight old key
				result = tGetButtonRect(gKeyHighlight,&old_r);
				if (!result) {
					gKeyHighlight = -1;
					SetPort(win);
					InvalRect(&old_r);
				}
			}
			gKeyHighlight = i;
			gKeyTrigger = kDLMacTriggeredByKeyboard;
			gTargetKc = kc;
			
			DLOPPushButton(gCalc,tScanCodeToKeyCode(gKeyHighlight));
			DLLCDRender(gCalc,0);
			
			SetPort(win);
			//PaintRect(&r);
			InvalRect(&r);
			InvalRect(&gLCDRect);
			if (gDebug) {
				InvalRect(&gDebugRect);
			}
			break;
		}
	}
}

void tDrawKeyPad(WindowPtr w) {
	int i, y, x, cidx, sidx, ox, oy, right, bottom;
	Rect rct;
	const static Rect kpRect = {30,0,240,240};
	// Background
	TextFont(gChicago);
	TextSize(12);
	RGBBackColor(DL_MAC_BACKGROUNDS+kDLMacEmptyColor);
	EraseRect(&kpRect);
	for (y=0; y < N_ROWS; y++) {
		for (x=0; x < N_COLS; x++) {
			i = y * N_COLS + x - 4;
			if (i < 0) continue;
			cidx = DL_MAC_KEY_COLOR_CONFIG[i];
			sidx = DL_MAC_KEY_SIZE_CONFIG[i];
			if (cidx == -1)
				continue;
			if (gKeyHighlight == i)
				RGBForeColor(DL_MAC_FOREGROUNDS+cidx);
			else
				RGBForeColor(DL_MAC_BACKGROUNDS+cidx);
			ox = DL_MAC_KEYPAD_GRID_X[x];
			oy = DL_MAC_KEYPAD_GRID_Y[y];
			right = ox + DL_MAC_BUTTON_WIDTH[sidx];
			bottom = oy + DL_MAC_BUTTON_HEIGHT[sidx];
			SetRect(&rct,ox,oy,right,bottom);
			PaintRect(&rct);
			if (gKeyHighlight == i)
				RGBForeColor(DL_MAC_BACKGROUNDS+cidx);
			else
				RGBForeColor(DL_MAC_FOREGROUNDS+cidx);
			MoveTo(ox+DL_MAC_LABEL_OFFSET_X[i], oy+DL_MAC_LABEL_OFFSET_Y[i]);
			DrawString((unsigned const char*)(DL_MAC_LABELS[i]));
		}
	}
}

void tClearLCD() {
	memset(gLCDBits,0,900);
}


void tDrawLCD(WindowPtr w) {
	const static Rect lcdRegion = {0,0,30,240};
	Rect iconRect = {0,0,16,16};
	SetPort(w);
	RGBBackColor(&DL_LCD_BACKGROUND);
	RGBForeColor(&DL_LCD_FOREGROUND);
	EraseRect(&lcdRegion);
	if (gCalc->power == 0) return;
	CopyBits(&gLCDBmp,&w->portBits,&gLCDRect,&gLCDRect,srcCopy,NULL);
}

void tDrawDebugInfo(WindowPtr w) {
	static Str255 buf;
	static char buf2[255];
	static char* ptr = (char*)buf;
	int len;
	SetPort(w);
	BackColor(blackColor);
	ForeColor(greenColor);
	TextFont(4);
	TextSize(9);
	EraseRect(&gDebugRect);
	ptr[0] = sprintf(ptr+1,"PWR=%d ST=%d CB=%d RM=%d FM=%d", gCalc->power, gCalc->state, gCalc->currentBinary, gCalc->roundMode, gCalc->fixMode);
	MoveTo(gDebugRect.left+1,gDebugRect.top+8);
	DrawString(buf);
	DLFormatDLNumber(&gCalc->arg1,buf2);
	ptr[0] = sprintf(ptr+1,"%s",buf2);
	MoveTo(gDebugRect.left+1,gDebugRect.top+16);
	DrawString(buf);
	DLFormatDLNumber(&gCalc->mem,buf2);
	ptr[0] = sprintf(ptr+1,"%s",buf2);
	MoveTo(gDebugRect.left+1,gDebugRect.top+24);
	DrawString(buf);
	DLFormatDLNumber(&gCalc->gt,buf2);
	ptr[0] = sprintf(ptr+1,"%s",buf2);
	MoveTo(gDebugRect.left+1,gDebugRect.top+32);
	DrawString(buf);
}

void tDraw(WindowPtr w) {
	SetPort(w);
	tDrawKeyPad(w);
	tRenderLCD();
	tDrawLCD(w);
	if (gDebug) {
		tDrawDebugInfo(w);
	}
	if (gFreezeScreen) {
		Rect bg = {0,0,15,240};
		BackColor(redColor);
		ForeColor(whiteColor);
		TextFont(gChicago);
		TextSize(12);
		EraseRect(&bg);
		MoveTo(0,12);
		DrawString(errmsg_buffer);
	}
}

void tUpdate(EventRecord *evt) {
	WindowPtr w = (WindowPtr)evt->message;
	if (w == win) {
		BeginUpdate(w);
		tDraw(w);
		EndUpdate(w);
	}
}

void tProcessEvent(EventRecord *evt) {
	switch (evt->what) {
		case mouseDown:
			tMouseDown(evt);
			break;
		case mouseUp:
			tMouseUp(evt);
			break;
		case keyDown:
			tKeyDown(evt);
			break;
		case updateEvt:
			tUpdate(evt);
			break;
	}
}

void tIdle(EventRecord *evt) {
	static KeyMap km;
	static unsigned char *km_ptr = (unsigned char *)km;
	static Rect statusRect = {0,0,30,20};
	static unsigned char tBlink = 0;
	static unsigned char prevBlink = 1;
	WindowPtr w;
	w = FrontWindow();
	if (w != win) return;
	if (gKeyTrigger == kDLMacTriggeredByKeyboard) {
		GetKeys(km);
		if ((km_ptr[gTargetKc/8] & (1 << (gTargetKc % 8))) == 0) {
			tKeyUp(evt);
		}
	}
	if (gCalc->state == DS_ERROR_GT || gCalc->state == DS_ERROR_M) {
		SetPort(win);
		GetDateTime(&time_now);
		tBlink = (time_now % 2) ? 255 : 0;
		if (tBlink != prevBlink) {
			prevBlink = tBlink;
			DLLCDRender(gCalc,tBlink);
			tRenderLCD();
			// InvalRect(&gLCDRect);
			RGBBackColor(&DL_LCD_BACKGROUND);
			RGBForeColor(&DL_LCD_FOREGROUND);
			CopyBits(&gLCDBmp,&w->portBits,&gLCDRect,&gLCDRect,srcCopy,NULL);
			if (gDebug) {
				Rect r = {0,0,12,240};
				Str255 now;
				char *ptr = (char *)now;
				TextFont(4);
				TextSize(9);
				BackColor(blueColor);
				ForeColor(whiteColor);
				EraseRect(&r);
				ptr[0] = sprintf(ptr+1,"GT/M Error. TIME = %ld",time_now);
				MoveTo(0,10);
				DrawString(now);
			}
		}
	}
}

void tMainLoop() {
	EventRecord evt;
	int gotEvent;
	while (!gShouldEnd) {
		gotEvent = WaitNextEvent(everyEvent,&evt,APP_SLEEP,NULL);
		if (gotEvent)
			tProcessEvent(&evt);
		else
			tIdle(&evt); // Key release goes here
	}
}

void tCreateMenu() {
	mbar = GetNewMBar(kMBar);
	if (!mbar) {
		SysBeep(30);
		ExitToShell();
	}
	SetMenuBar(mbar);
	DisposHandle(mbar);
	AppendResMenu(GetMenuHandle(kAppleMenu),'DRVR');
	DrawMenuBar();
}

int AppError(char *msg, ...) {
	va_list arglist;
	char *ptr = (char *)errmsg_buffer;
	va_start(arglist,msg);
	ptr[0] = vsprintf(ptr+1,msg,arglist);
	va_end(arglist);
	ptr = (char *)errmsg_buffer+1;
	while (*ptr) {
		if (*ptr < 0x20 || *ptr > 0x7E) *ptr = '.';
		ptr++;
	}
	gFreezeScreen = 1;
	SetPort(win);
	InvalRect(&win->portRect);
}

void tCreateCalc() {
	gCalc = DLNew();
	if (!gCalc) {
		NoteAlert(kAlertDLFail,NULL);
		ExitToShell();
	}
}

main()
{
	tClearLCD();
	InitToolbox();
	// NoteAlert(kDialogCQD,NULL);
	GetDateTime(&time_app_start);
	CheckSysRequirements();
	tCreateCalc();
	CreateWindow();
	tCreateMenu();
	tMainLoop();
	return 0;
}
