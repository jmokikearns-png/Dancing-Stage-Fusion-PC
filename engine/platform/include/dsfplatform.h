/* dsfplatform - StepMania 2005 donor platform layer for the Dancing Stage Fusion PC port.
 *
 * Provides, from the original StepMania 2005-06-19 sources:
 *   - keyboard input        (X11 on Linux, DirectInput 5 on Windows)
 *   - dance pad / joystick  (/dev/input/js* on Linux, DirectInput 5 on Windows)
 *   - input filtering       (InputFilter: press/release edges with timestamps)
 *   - mapping               (InputMapper: up to 5 physical inputs per pad button,
 *                            2 pads, persisted to <config dir>/Data/Keymaps.ini)
 *   - window / GL context   (LowLevelWindow_X11 / _Win32 through RageDisplay_OGL:
 *                            windowed/fullscreen, mode switch, vsync on WGL)
 *
 * The game itself (the original PS1 code running on the port's runtime) consumes
 * the result as PS1 digital-pad state: GetPadWord() returns the 16-bit button
 * word exactly as a SCPH-1080 digital pad sends it over SIO (active low).
 *
 * This header deliberately exposes no StepMania types except RageDisplay, so the
 * runtime is not coupled to the engine's headers.
 */
#ifndef DSFPLATFORM_H
#define DSFPLATFORM_H

#include <stdint.h>
#include <string>
#include <vector>

class RageDisplay;

namespace dsf
{
	/* PS1 digital pad buttons, in the order the mapping file lists them. */
	enum Ps1Button
	{
		PS1_UP, PS1_DOWN, PS1_LEFT, PS1_RIGHT,
		PS1_CROSS, PS1_CIRCLE, PS1_SQUARE, PS1_TRIANGLE,
		PS1_L1, PS1_L2, PS1_R1, PS1_R2,
		PS1_START, PS1_SELECT,
		NUM_PS1_BUTTONS
	};
	enum { NUM_PADS = 2, NUM_SLOTS = 5 };

	/* Bit of each button in the pad's 16-bit SIO response (bytes 3..4 of the
	 * 0x41 digital pad reply, little-endian; a 0 bit means pressed). */
	int Ps1ButtonBit( Ps1Button b );
	const char *Ps1ButtonName( Ps1Button b );	/* "Up", "Cross", ... as in Keymaps.ini */

	struct Config
	{
		Config();
		std::string sConfigDir;		/* mounted as "/"; Keymaps.ini goes in <dir>/Data/ */
		std::string sInputDrivers;	/* "" = platform default ("X11,Joystick" / "DirectInput") */
		bool bOpenWindow;		/* false: input only (Linux X11 input still needs a window) */
		bool bWindowed;
		int iWidth, iHeight;		/* window / fullscreen mode size */
		int iRefreshRate;		/* 0 = default */
		bool bVsync;
		int iLogicalWidth, iLogicalHeight;	/* RageDisplay's SCREEN_WIDTH/HEIGHT */
		std::string sWindowTitle;
		bool bThreadedInput;		/* DirectInput: buffered input thread (SM default: on) */
		bool bLogToStdout;
	};

	/* One mapped edge: a physical input changed state and it is mapped to a pad button. */
	struct PadEvent
	{
		int iPad;		/* 0 or 1 */
		Ps1Button button;
		bool bPressed;
		double fTimestamp;	/* seconds since start (RageTimer); from the driver thread when it has one */
		std::string sDeviceInput;	/* e.g. "Key_x", "Joy1_B3" */
	};

	/* An unmapped physical input edge (useful for a "press a key to assign" UI). */
	struct RawEvent
	{
		bool bPressed;
		double fTimestamp;
		std::string sDeviceInput;
	};

	/* Input-only builds (DSF_INPUT_ONLY): the host owns the window.  On Windows,
	 * DirectInput needs that top-level window (HWND) for its cooperative level and
	 * only accepts keyboard input while it is the foreground window.  Call before
	 * Init(); a no-op on other platforms. */
	void SetHostWindow( void *pNativeWindow );

	bool Init( int argc, char **argv, const Config &cfg, std::string &sErrorOut );
	void Shutdown();	/* note: also saves the mapping (~InputMapper does, as in StepMania) */

	/* Pump window messages and input once (call once per host frame). */
	void Update( float fDeltaSeconds );
	void GetEvents( std::vector<PadEvent> &vPadOut, std::vector<RawEvent> &vRawOut );

	bool IsPressed( int iPad, Ps1Button b );
	uint16_t GetPadWord( int iPad );		/* active-low PS1 button word */

	/* Mapping (InputMapper).  Slot 0..4; slot 2 holds the built-in defaults. */
	bool SetMapping( int iPad, Ps1Button b, int iSlot, const std::string &sDeviceInput );
	void ClearMapping( int iPad, Ps1Button b, int iSlot );
	std::string GetMapping( int iPad, Ps1Button b, int iSlot );
	void ResetMappingToDefaults();
	void LoadMapping();
	void SaveMapping();
	std::string KeymapsPath();	/* real path of Keymaps.ini */

	/* Devices found by the drivers ("Keyboard", joystick product names). */
	void GetDevices( std::vector<std::string> &vOut );

	RageDisplay *Display();		/* NULL if bOpenWindow was false */
	bool QuitRequested();		/* window closed */
	bool HasFocus();
}

#endif
