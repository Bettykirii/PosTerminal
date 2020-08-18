//****************************************************************************
//       INGENICO                                INGEDEV 7
//============================================================================
//       FILE  SCHEMES.C                          (Copyright INGENICO 2012)
//============================================================================
//  Created :       16-July-2012     Kassovic
//  Last modified : 16-July-2012     Kassovic
//  Module : TRAINING
//
//  Purpose :
//  Security DLL management: Secret area (booster) + pinpad (P30 PP30S)
//      Helpful component to manage secure schemes.
//      IAPP.lib + SEC.LIB must be implemented to link properly.
//      Add SEC_lib_Open inside Entry.c (void entry(void))
//       Show how to load a Iso9564Pin Key + enter pin + get pinblock.
//       Show how to load a Mac Key + get Mac on a message.
//
//  List of routines in file :
//      ChkPpdConfig : Check Manager configuration.
//      GetPpdDisplay : Get type of Pinpad's display.
//      IsPinOnPpd : Pin entry on pinpad?.
//      VarId : Value Added Reseller id (Only Booster1).
//      SerialNb : Serial number of the signature card (Only Booster1).
//      AreaId : Secret area id.
//      PinEntry : Manage the Pin Entry.
//      SetConfig : Set configuration of the security component.
//      GetConfig : Get configuration of the security component.
//      CreateSecret : Secret area creation.
//      FreeSecret : Secret area deletion.
//      LoadIso9564Key : Load ISO9564Pin Key inside the secret area.
//      LoadMacKey : Load Mac Key inside the secret area.
//      Iso9564PinCiphering : Iso9564Pin enciphered by PIN key.
//      Iso9564PinEntry : Manage the ISO9564Pin entry to get the Pinblock.
//      MacCalculation : Manage the Mac on a message.
//
//  File history :
//  071612-BK : File created
//
//****************************************************************************

//****************************************************************************
//      INCLUDES
//****************************************************************************
#include "Training.h"
#include "globals.h"
#include "pp30_def.h"
#include "SEC_interface.h"
#ifdef __TELIUM3__
#include "SchemeErrors.h"
#endif
#include "schVar_def.h"
#include "tlvVar_def.h"

//****************************************************************************
//      EXTERN
//****************************************************************************
extern T_GL_HGRAPHIC_LIB gGoalGraphicLibInstance; // Handle of the graphics object library

//****************************************************************************
//      PRIVATE CONSTANTS
//****************************************************************************
#ifndef __TELIUM3__
#define C_NB_PARTS  3             // 1-pincode entries 2-card reader access 3-ciphering functions
#else
#define C_NB_PARTS  5             // 1-pincode entries 2-card reader access 3-ciphering functions
#endif                                // 4-keyboard entries (future) 5-display functions (future)

#define MAX_CHOICE  2
#define NUMBER_OF_ITEMS(a) (sizeof(a)/sizeof((a)[0]))
#define COLOR_CUSTOM        0xFF27B9CF

#define ECHO_PIN   '*'

// Secret area identification
// ==========================
// -----------------------------------------------------------
// *** On DEBUG Mode => Terminal mockup with profile DEBUG ***
// -----------------------------------------------------------
// == Booster1 (T1) ==
// VAR_ID = 0x00FA         // Var Id from signing card T10 (only for Booster1, Booster2&3 retrieved from certificate)
// CARD_NB =  0x0003C009   // Serial number from RSA signing card T10 (only for Booster1, Booster2&3 retrieved from certificate)
// See functions VarId() and SerialNb()
// == Booster2 (T1) ==
// Var Id = 0x00FA & Serial number = 0x0003C009  retrieved from Certificate with debug profile [T10].
// Example of AREA_ID = 0x00002130    // Secret area identification from 00002130 to 3FFF2F3F
// (2 first bytes from 0000 to 3FFF)
// See function AreaId()

// == Booster3 (T2) ==
// Var Id = 0x010D & Serial number = 0x0003C00E retrieved from Certificate with debug profile [0035-001].
// Example of AREA_ID = 0x010D2130    // Secret area identification from 00002130 to 3FFF2F3F
// (2 first bytes from 0000 to 3FFF)
// See function AreaId()

// == Trusted Zone (TETRA) ==
// ASID = 0x800C & Serial number = unknown retrieved from Certificate with ESSP 00000004.
// Example of AREA_ID = 0x800C2130    // Secret area identification from 00002130 to FFFF2F3F
// (2 first bytes from 0000 to FFFF)
// See function AreaId()

// ----------------------------------------------------------------
// *** On RELEASE Mode => Terminal production with profile TEST ***
// ----------------------------------------------------------------
// == Booster1 (T1) ==
// VAR_ID = 0x0000         // Var Id from RSA signing card [Mine is S17] (only for Booster1, Booster2&3 retrieved from certificate)
// CARD_NB = 0x0003C00F    // Serial number RSA signing card S17 (only for Booster1, Booster2&3 retrieved from certificate)
// See functions VarId() and SerialNb()
// == Booster2 (T1) ==
// Var Id = 0x0000 & Serial number = 0x0003C00F retrieved from Certificate [My profile is S17].
// Example of AREA_ID = 0x00002130    // Secret area identification from 00002130 to 3FFF2F3F
// (2 first bytes from 0000 to 3FFF)
// See function AreaId()

// == Booster3 (T2) ==
// Var Id = 0x0119 & Serial number = 000104EC retrieved from Certificate [My profile is 0034-001].
// Example of AREA_ID =  0x01192130   // Secret area identification from 00002130 to 3FFF2F3F
// (2 first bytes from 0000 to 3FFF)
// See function AreaId()

// == Trusted Zone (TETRA) ==
// ASID = 0x0010 & Serial number = 000000A6 retrieved from Certificate with ESSP 00000004.
// Example of AREA_ID = 0x00102130    // Secret area identification from 00002130 to FFFF2F3F
// (2 first bytes from 0000 to FFFF)
// See function AreaId()


// Key(s) Location                       // Location into secret area
// ===============
#define ROOT_KEY_LOC	      8*4        // Root Key                   252 slot => 1008 position bit
#define ISO9564PIN_KEY_LOC   38*4        // Iso9564Pin Key               2 slot =>    8 position bit
#define MAC_KEY_LOC          55*4        // Mac Key                     22 slot =>   88 position bit

// Bank(s) Id                            // Bank identification from 80000xyy to FFFFxxyy related to a key loaded into secret area
// ==========                            // xx (simple or double DES key) yy (key location in hex by 4 bits)
#define BANK_ROOT_ID 		0x800002FC   // 252*4 => 0xFC*4
#define BANK_ISO9564_ID     0x80000202   // 2*4   => 0x02*4
#define BANK_MAC_ID         0x80000116   // 22*4  => 0x16*4

//****************************************************************************
//      PRIVATE TYPES
//****************************************************************************
/* */

//****************************************************************************
//      PRIVATE DATA
//****************************************************************************
// Properties of the Sec screen (Goal)
// ===================================
static const ST_DSP_LINE txSec[] =
{
		{ {GL_ALIGN_CENTER, GL_ALIGN_CENTER, GL_COLOR_YELLOW, GL_COLOR_TRANSPARENT, 0, FALSE, {0, 0, 0, 0}, {0, 0, 0, 0, GL_COLOR_WHITE}, {0, 0, 0, 0}, {NULL,  GL_FONT_STYLE_NORMAL, GL_SCALE_SMALL}}, // Line0
				{GL_ALIGN_CENTER, GL_ALIGN_CENTER, TRUE,  0, FALSE, {3, 3, 2, 1}, {0, 0, 0, 0, GL_COLOR_WHITE}, {0, 0, 0, 0}, {NULL, GL_FONT_STYLE_NORMAL, GL_SCALE_SMALL}} },
				{ {GL_ALIGN_CENTER, GL_ALIGN_CENTER, GL_COLOR_YELLOW, GL_COLOR_TRANSPARENT, 0, FALSE, {0, 0, 0, 0}, {0, 0, 0, 0, GL_COLOR_WHITE}, {0, 0, 0, 0}, {NULL, GL_FONT_STYLE_NORMAL, GL_SCALE_XSMALL}}, // Line1
						{GL_ALIGN_CENTER, GL_ALIGN_CENTER, FALSE, 0, FALSE, {0, 2, 0, 0}, {0, 0, 0, 0, GL_COLOR_WHITE}, {0, 0, 0, 0}, {NULL, GL_FONT_STYLE_NORMAL, GL_SCALE_XSMALL}} },
						{ {GL_ALIGN_CENTER, GL_ALIGN_CENTER, GL_COLOR_WHITE,  GL_COLOR_TRANSPARENT, 0, FALSE, {0, 0, 0, 0}, {0, 0, 0, 0, GL_COLOR_WHITE}, {0, 0, 0, 0}, {NULL,  GL_FONT_STYLE_NORMAL, GL_SCALE_SMALL}}, // Line2
								{GL_ALIGN_CENTER, GL_ALIGN_CENTER, TRUE,  0, FALSE, {3, 3, 2, 1}, {0, 0, 0, 0, GL_COLOR_WHITE}, {0, 0, 0, 0}, {NULL, GL_FONT_STYLE_NORMAL, GL_SCALE_SMALL}} },
								{ {GL_ALIGN_CENTER, GL_ALIGN_CENTER, GL_COLOR_WHITE,  GL_COLOR_TRANSPARENT, 0, FALSE, {0, 0, 0, 0}, {0, 0, 0, 0, GL_COLOR_WHITE}, {0, 0, 0, 0}, {NULL, GL_FONT_STYLE_NORMAL, GL_SCALE_XSMALL}}, // Line3
										{GL_ALIGN_CENTER, GL_ALIGN_CENTER, FALSE, 0, FALSE, {0, 2, 0, 0}, {0, 0, 0, 0, GL_COLOR_BLACK}, {0, 0, 0, 0}, {NULL, GL_FONT_STYLE_NORMAL, GL_SCALE_XSMALL}} },
										{ {GL_ALIGN_CENTER, GL_ALIGN_CENTER, COLOR_CUSTOM,    GL_COLOR_TRANSPARENT, 0, FALSE, {0, 0, 0, 0}, {0, 0, 0, 0, GL_COLOR_WHITE}, {0, 0, 0, 0}, {NULL,  GL_FONT_STYLE_NORMAL, GL_SCALE_SMALL}}, // Line4
												{GL_ALIGN_CENTER, GL_ALIGN_CENTER, TRUE,  0, FALSE, {3, 3, 2, 1}, {0, 0, 0, 0, GL_COLOR_BLACK}, {0, 0, 0, 0}, {NULL, GL_FONT_STYLE_NORMAL, GL_SCALE_SMALL}} },
												{ {GL_ALIGN_CENTER, GL_ALIGN_CENTER, COLOR_CUSTOM,    GL_COLOR_TRANSPARENT, 0, FALSE, {0, 0, 0, 0}, {0, 0, 0, 0, GL_COLOR_WHITE}, {0, 0, 0, 0}, {NULL, GL_FONT_STYLE_NORMAL, GL_SCALE_XSMALL}}, // Line5
														{GL_ALIGN_CENTER, GL_ALIGN_CENTER, FALSE, 0, FALSE, {0, 2, 0, 0}, {0, 0, 0, 0, GL_COLOR_BLACK}, {0, 0, 0, 0}, {NULL, GL_FONT_STYLE_NORMAL, GL_SCALE_XSMALL}} }
};

// Properties of the Pin screen (Goal)
// ===================================
static const ST_DSP_LINE txPin[] =
{
		{ {GL_ALIGN_CENTER, GL_ALIGN_RIGHT,  GL_COLOR_WHITE, GL_COLOR_TRANSPARENT,   0, FALSE, {0, 0, 1, 0}, {0, 0, 0, 0, GL_COLOR_WHITE}, {0, 0, 0, 0}, {NULL, GL_FONT_STYLE_NORMAL, GL_SCALE_XLARGE}}, // Line0
				{GL_ALIGN_CENTER, GL_ALIGN_RIGHT,  FALSE,  0, FALSE, {0, 0, 1, 0}, {0, 0, 0, 0, GL_COLOR_WHITE}, {0, 0, 0, 0}, {NULL, GL_FONT_STYLE_NORMAL, GL_SCALE_XLARGE}} },
				{ {GL_ALIGN_LEFT,   GL_ALIGN_LEFT,   GL_COLOR_WHITE, GL_COLOR_TRANSPARENT,   0, FALSE, {1, 0, 0, 0}, {0, 0, 0, 0, GL_COLOR_WHITE}, {0, 0, 0, 0}, {NULL, GL_FONT_STYLE_NORMAL, GL_SCALE_XLARGE}}, // Line1
						{GL_ALIGN_LEFT,   GL_ALIGN_LEFT,   FALSE,  0, FALSE, {1, 0, 0, 0}, {0, 0, 0, 0, GL_COLOR_WHITE}, {0, 0, 0, 0}, {NULL, GL_FONT_STYLE_NORMAL, GL_SCALE_XLARGE}} },
						{ {GL_ALIGN_CENTER, GL_ALIGN_CENTER, GL_COLOR_BLACK, GL_COLOR_WHITE, 100,  TRUE, {0, 0, 0, 0}, {0, 0, 0, 0, GL_COLOR_WHITE}, {2, 0, 2, 2}, {NULL, GL_FONT_STYLE_NORMAL, GL_SCALE_MEDIUM}}, // Line2
								{GL_ALIGN_CENTER, GL_ALIGN_CENTER, TRUE, 100,  TRUE, {0, 2, 0, 2}, {0, 0, 0, 0, GL_COLOR_WHITE}, {0, 0, 0, 0}, {NULL, GL_FONT_STYLE_NORMAL, GL_SCALE_MEDIUM}} }
};

// Properties of the Result screen (Goal)
// ======================================
static const ST_DSP_LINE txResult[] =
{
		{ {GL_ALIGN_CENTER, GL_ALIGN_CENTER, GL_COLOR_WHITE, GL_COLOR_TRANSPARENT,   0, FALSE, {0, 0, 0, 0}, {0, 0, 0, 0, GL_COLOR_WHITE}, {0, 0, 0, 0}, {NULL,  GL_FONT_STYLE_NORMAL, GL_SCALE_XLARGE}}, // Line0
				{GL_ALIGN_CENTER, GL_ALIGN_CENTER, FALSE,  0, FALSE, {0, 0, 0, 0}, {0, 0, 0, 0, GL_COLOR_BLACK}, {0, 0, 0, 0}, {NULL,  GL_FONT_STYLE_NORMAL, GL_SCALE_XLARGE}} },
				{ {GL_ALIGN_CENTER, GL_ALIGN_CENTER, GL_COLOR_WHITE, GL_COLOR_RED,    80, FALSE, {0, 6, 0, 6}, {1, 1, 1, 1, GL_COLOR_WHITE}, {0, 0, 0, 0}, {NULL,  GL_FONT_STYLE_NORMAL, GL_SCALE_LARGE}},  // Line1
						{GL_ALIGN_CENTER, GL_ALIGN_CENTER, TRUE,  90, FALSE, {0, 8, 0, 8}, {0, 0, 0, 0, GL_COLOR_BLACK}, {0, 0, 0, 0}, {NULL,  GL_FONT_STYLE_NORMAL, GL_SCALE_MEDIUM}} }
};

// Properties of the default printer (Goal)
// ========================================
static const ST_PRN_LINE xPrinter =
{
		GL_ALIGN_LEFT, GL_ALIGN_CENTER, FALSE, 100, {0, 0, 0, 0}, {0, 0, 0, 0, GL_COLOR_BLACK}, {1, 0, 1, 0}, {NULL, GL_FONT_STYLE_NORMAL, GL_SCALE_LARGE}
};

// SKMT2 T_DES (CBC mode)
// RootKey => (PartialKey1=213F2F5633B199A111A7F8BDEC6D6FBB) deciphered by CKey (PartialKey2=0123456789ABCDEFFEDCBA9876543210)
const byte zRootKeyPlainText[] = "\x11\x11\x11\x11\x11\x11\x11\x11""\x22\x22\x22\x22\x22\x22\x22\x22";

// Iso9564PinKey ciphered by Root key => deciphered is 6B218F24DE7DC66C6B218F24DE7DC66C
const byte zIso9564PinKey[] = "\x40\xB3\x9B\xA8\x55\xE0\x41\x16""\x40\xB3\x9B\xA8\x55\xE0\x41\x16";

// MacKey ciphered by Root key => deciphered is 0923456789ABCDEF
const byte zMacKey[] = "\x27\xB6\xA9\x4E\x50\xD4\xFA\x45";

// Dialog choice
// =============
static const char *tzMenuChoice[]=
{
		"Terminal",
		"PinPad",
		NULL
};

//****************************************************************************
//                     int ChkPpdConfig (void)
//  This function checks the pinpad configuration from the Manager.
//  This function has no parameters.
//  This function has return value.
//    >=0 : Right configuration (No pinpad=0 or pinpad present>0 1:B1 2:B2, 3:B3)
//     <0 : Wrong configuration (Wrong pinpad or pinpad missing but required=-1)
//****************************************************************************

static int ChkPpdConfig(void)
{
	// Local variables
	// ***************
	T_GL_HWIDGET hDocument=NULL;
	ST_PRN_LINE xLine;
	byte p; // Printer line index
	char tcReference[17+1];
	int iRet;

	// Check pinpad configuration from Manager
	// ***************************************
	iRet = PSQ_Is_pinpad();
	if (iRet != 0)                                              // Manager asks for a pinpad
	{
		memset(tcReference, 0, sizeof(tcReference));
		iRet = IsPPSConnected(); CHECK(iRet==RET_PP_OK, lblKO); // Check if pinpad is connected
		PPS_SendTerminalRefRequestCommand((byte*)tcReference);  // Get pinpad reference (Bug ret is always 0)
		switch (PSQ_Pinpad_Value())
		{
		case PP30_PPAD:                                         // Pinpad characters no-reader Booster1
			CHECK (memcmp(&tcReference[8], "PP30 ", 5)==0, lblKO);
			iRet=BOOSTER_1;
			break;
		case PP30S_PPAD:                                        // Pinpad characters no-reader Booster2
			CHECK (memcmp(&tcReference[8], "PP30S", 5)==0, lblKO);
			iRet=BOOSTER_2;
			break;
		case PPC30_PPAD:                                        // Pinpad graphic no-reader Booster1
			CHECK (memcmp(&tcReference[8], "PPC30", 5)==0, lblKO);
			iRet=BOOSTER_1;
			break;
		case PPR30_PPAD:                                        // Pinpad graphic reader Booster1
			CHECK (memcmp(&tcReference[8], "PPR30", 5)==0, lblKO);
			iRet=BOOSTER_1;
			break;
		case P30_PPAD:                                          // Pinpad graphic reader Booster2
			CHECK (memcmp(&tcReference[8], "P30  ", 5)==0, lblKO);
			iRet=BOOSTER_2;
			break;
		case IPP220_PPAD:                                       // Pinpad character no-reader Booster3
			CHECK (memcmp(&tcReference[8], "IPP2XX", 6)==0, lblKO);
			iRet=BOOSTER_3;
			break;
		case IPP250_PPAD:                                       // Pinpad character no-reader color Booster3
			CHECK (memcmp(&tcReference[8], "IPP2XX", 6)==0, lblKO);
			iRet=BOOSTER_3;
			break;
		case IPP280_PPAD:                                       // Pinpad graphic no-reader color Booster3
			CHECK (memcmp(&tcReference[8], "IPP2XX", 6)==0, lblKO);
			iRet=BOOSTER_3;
			break;
		case IPP320_PPAD:                                       // Pinpad graphic reader Booster3
			CHECK (memcmp(&tcReference[8], "IPP3XX", 6)==0, lblKO);
			iRet=BOOSTER_3;
			break;
		case IPP350_PPAD:                                       // Pinpad graphic reader color Booster3
			CHECK (memcmp(&tcReference[8], "IPP3XX", 6)==0, lblKO);
			iRet=BOOSTER_3;
			break;
		default:                                                // Pinpad not supported
			GL_Dialog_Message(gGoalGraphicLibInstance, NULL, "Pin Pad Unknown", GL_ICON_WARNING, GL_BUTTON_VALID, 5*1000);
			iRet=-1;
			break;
		}
	}

	goto lblEnd;                                                // Right pinpad configuration (No pinpad or pinpad present)

	// Errors treatment
	// ****************
	lblKO:                                                          // Pinpad required (pinpad disconnected or missing)
	GL_Dialog_Message(gGoalGraphicLibInstance, NULL, "Pin Pad Required\nCheck Configuration", GL_ICON_ERROR, GL_BUTTON_NONE, 0);

	hDocument = GoalCreateDocument(gGoalGraphicLibInstance, GL_ENCODING_UTF8);    // Create document
	if (hDocument != NULL)
	{
		p=0;
		xLine = xPrinter;
		xLine.eBackAlign = GL_ALIGN_LEFT;
		xLine.xFont.eScale = GL_SCALE_XLARGE;
		xLine.xFont.eStyle = GL_FONT_STYLE_BOLD;                // Requirement
		GoalPrnLine(hDocument, p++, "Pinpad problem!!!", &xLine);
		xLine.xFont.eScale = GL_SCALE_LARGE;
		xLine.xFont.eStyle = GL_FONT_STYLE_NORMAL;
		GoalPrnLine(hDocument, p++, "     ----------------------------------\n"
				"Pinpad disconnected\n"
				"=> plug-it again\n"
				"     -------------- OR --------------\n"
				"Wrong pinpad connected\n"
				"=> Check MANAGER config\n"
				"     -------------- OR --------------\n"
				"Pinpad required\n"
				"=> Check MANAGER config\n"
				"     ----------------------------------\n"
				"\n\n\n\n", &xLine);
		GoalPrnDocument(hDocument);                             // Print document

		GoalDestroyDocument(&hDocument);                        // Destroy document
	}
	iRet=-1;
	goto lblEnd;
	lblEnd:
	return iRet;
}

//****************************************************************************
//                     int GetPpdDisplay (void)
//  This function returns the type of pinpad's display.
//  This function has no parameters.
//  This function has return value.
//     2 : Pinpad with graphical display
//     1 : Pinpad with character display
//     0 : No pinpad connected
//    -1 : Processing error
//****************************************************************************

int GetPpdDisplay(void)
{
	// Local variables
	// ***************
	int iDisplay;
	int iRet;

	// Check if pinpad declared from Manager and connected to the terminal
	// *******************************************************************
	iRet = PSQ_Is_pinpad(); CHECK(iRet!=0, lblKO);
	iRet = IsPPSConnected(); CHECK(iRet==RET_PP_OK, lblKO);

	// Retrieve the type of display
	// ****************************
	iRet = PSQ_Pinpad_Value();
	switch (iRet)
	{
	case PP30_PPAD:          // Pinpad CHARACTERS no-reader Booster1
	case PP30S_PPAD:         // Pinpad CHARACTERS no-reader Booster2
	case IPP220_PPAD:        // Pinpad CHARACTERS no-reader Booster3
	case IPP250_PPAD:        // Pinpad CHARACTERS no-reader color Booster3
		iDisplay=1;          // Display = CHARACTERS
		break;
	case PPC30_PPAD:         // Pinpad GRAPHIC no-reader Booster1
	case PPR30_PPAD:         // Pinpad GRAPHIC reader Booster1
	case P30_PPAD:           // Pinpad GRAPHIC reader Booster2
	case IPP280_PPAD:        // Pinpad GRAPHIC no-reader color Booster3
	case IPP320_PPAD:        // Pinpad GRAPHIC reader Booster3
	case IPP350_PPAD:        // Pinpad GRAPHIC reader color Booster3
	case IPP310_PPAD:        // Pinpad GRAPHIC reader Booster3 for TETRA
	case IPP315_PPAD:        // Pinpad GRAPHIC reader color Booster3 for TETRA
		iDisplay=2;          // Display = GRAPHIC
		break;
	default:
		iDisplay=-1;         // Pinpad unknown
		break;
	}

	goto lblEnd;

	// Errors treatment
	// ****************
	lblKO:
	iDisplay=0;              // No pinpad connected
	goto lblEnd;
	lblEnd:
	return iDisplay;
}

//****************************************************************************
//                     int IsPinOnPpd (void)
//  This function checks if the Pin entry is done on terminal or on Pinpad.
//  This function has no parameters.
//  This function has return value.
//     2 : Pin entry on PinPad with graphical display
//     1 : Pin entry on pinpad with character display
//     0 : Pin entry on terminal
//    -1 : Processing error
//****************************************************************************

static int IsPinOnPpd(void)
{
	// Local variables
	// ***************
	T_SEC_CONFIG txConfParts[C_NB_PARTS];
	int iRet, iNbParts;

	// Get the configuration of the security component
	// ***********************************************
	iRet = SEC_GetConfig(&iNbParts, txConfParts);  // Get number and parameters of secure parts
	CHECK(iRet==OK, lblKO);

	// Analyze C_SEC_PINCODE secure part
	// *********************************
	iRet = GetPpdDisplay(); CHECK(iRet>=0, lblKO); // Pin entry on pinpad or terminal?
	if (iRet != 0)                                 // Pinpad connected
		if (memcmp(txConfParts[C_SEC_PINCODE-1].ptszBoosterPeripheral, IAPP_USB_NAME, strlen(IAPP_USB_NAME)) != 0)
			iRet=0;                                // Pin entry redirected to terminal

	goto lblEnd;

	// Errors treatment
	// ****************
	lblKO:
	iRet=-1;
	goto lblEnd;
	lblEnd:
	return iRet;
}

//****************************************************************************
//                        int VarId (void)
//  This function returns the VarId (Value Added Reseller ID).
//  The purpose of the VarId is to protect the access to secret area.
//  The VarId is saved inside the secret area.
//  Enter manually for Booster1. Retrieve automatically from certificate for
//  Booster2&3.
//  This function has no parameters.
//  This function has return value.
//    The VarId value.
//****************************************************************************
#ifndef __TELIUM3__
static int VarId (void)
{
	// Local variables
	// ***************
	int iVarId;

	// Value Added Reseller ID
	// ***********************
	if (strcmp (_ING_APPLI_SIGN_MODE, "Unsigned") == 0)
	{
		// =====================================================================
		// On DEBUG Mode => Terminal mockup with profile DEBUG
		//     VarId and SerialNumber from MANUFACTURER (INGENICO)
		// On Telium 1 (B1 only) VarId=0x00FA SerialNumber=0x0003C009 (Manually)
		// =====================================================================
		iVarId = 0x00FA;
	}
	else
	{
		// =====================================================================
		// On RELEASE Mode => Terminal production with profile TEST
		//            VarId and SerialNumber from CUSTOMER
		// On Telium 1 (B1 only) VarId=0x0000 SerialNumber=0x0003C00F (Manually)
		// =====================================================================
		iVarId = 0x0000;
	}

	return iVarId;
}
#endif
//****************************************************************************
//                        int SerialNb (void)
//  This function returns the SerialNb (Serial number of the signature card).
//  The purpose of the serial number is to protect the secret area.
//  The serial number is saved inside the secret area.
//  Enter manually for Booster1. Retrieve automatically from certificate for
//  Booster2&3.
//  This function has no parameters.
//  This function has return value.
//    The SerialNb value.
//****************************************************************************
#ifndef __TELIUM3__
static int SerialNb (void)
{
	// Local variables
	// ***************
	int iSerialNb;

	// Serial number of the signature card
	// ***********************************
	if (strcmp (_ING_APPLI_SIGN_MODE, "Unsigned") == 0)
	{
		// =====================================================================
		// On DEBUG Mode => Terminal mockup with profile DEBUG
		//     VarId and SerialNumber from MANUFACTURER (INGENICO)
		// On Telium 1 (B1) only VarId=0x0000 SerialNumber=0x0003C009 (Manually)
		// =====================================================================
		iSerialNb = 0x0003C009;
	}
	else
	{
		// =====================================================================
		// On RELEASE Mode => Terminal production with profile TEST
		//            VarId and SerialNumber from CUSTOMER
		// On Telium 1 (B1) only VarId=0x0000 SerialNumber=0x0003C00F (Manually)
		// =====================================================================
		iSerialNb = 0x0003C00F;
	}

	return iSerialNb;
}
#endif
//****************************************************************************
//                        int AreaId (void)
//  This function returns the AreaId (Secret Area ID).
//  The purpose of the area id is to provide a unique id to the secret area.
//  This function has no parameters.
//  This function has return value.
//    The AreaId value.
//****************************************************************************

static int AreaId (void)
{
	// Local variables
	// ***************
	T_SEC_CONFIG txConfParts[C_NB_PARTS];
	int iNbParts, iBoosterType, iAreaId, iRet;

	// Get the configuration of the security component
	// ***********************************************
	iRet = SEC_GetConfig(&iNbParts, txConfParts);  // Get number and parameters of secure parts
	if (iRet != OK) return 0;                      // No booster => no area id

	// Analyze C_SEC_CIPHERING secure part
	// ***********************************
	switch (txConfParts[C_SEC_CIPHERING-1].cBoosterType)
	{
	case C_SEC_BL1:    iBoosterType = BOOSTER_1;     break;   // Booster 1 (1)
	case C_SEC_BL2:                                           // Booster 2 or 3 (2 or 3)
		iBoosterType = ChkPpdConfig();    // Booster type from pinpad if plugged
		if (iBoosterType < 0)
			return 0;                     // Processing error => No VarId
		if (iBoosterType == 0)            // No pinpad
			iBoosterType = BoosterType(); // Booster type from terminal
		break;
#ifdef __TELIUM3__
	case C_SEC_ISLERO: iBoosterType = BOOSTER_3+1;   break;   // Secure unit islero (4)
#endif
	default:           iBoosterType = NO_BOOSTER;    break;   // Should not occur (0)
	}

	// Secret Area ID
	// **************
	if (strcmp (_ING_APPLI_SIGN_MODE, "Unsigned") == 0)
	{
		// =====================================================================
		// On DEBUG Mode => Terminal mockup with profile DEBUG
		//     VarId\ASID and SerialNumber from MANUFACTURER (INGENICO)
		// On Telium 1 (B1) VarId=0x00FA SerialNumber=0x0003C009 (Manually)
		// On Telium 1 (B2) VarId=0x00FA SerialNumber=0x0003C009 (Certificate)
		// On Telium 2 (B3) VarId=0x010D SerialNumber=0x0003C00E (Certificate)
		// Secret area identification from 00002130 to 3FFF2F3F
		//             (2 first bytes from 0000 to 3FFF)
		// On Tetra    (ISLERO) ASID=0x800C SerialNumber=Unknown (Certificate)
		// Secret area identification from 00002130 to FFFF2F3F
		//             (2 first bytes from 0000 to FFFF)
		// =====================================================================
		switch (iBoosterType)
		{
		case BOOSTER_1:
		case BOOSTER_2:
			// Could be VarId=0x00FA but NOT MANDATORY + 2&3 fixed + 1&0 user
			iAreaId = 0x00FA2130;
			break;
		case BOOSTER_3:
			// Could be VarId=0x0000 but NOT MANDATORY + 2&3 fixed + 1&0 user
			iAreaId = 0x010D2130;
			break;
		case BOOSTER_3+1:
		// Could be ASID=0x800C but NOT MANDATORY + 2&3 fixed + 1&0 user
		iAreaId = 0x015C2131;
		break;
		default:
			iAreaId = 0;
			break;
		}
	}
	else
	{
		// =====================================================================
		// On RELEASE Mode => Terminal production with profile TEST
		//            VarId\ASID and SerialNumber from CUSTOMER
		// On Telium 1 (B1) VarId=0x0000 SerialNumber=0x0003C00F (Manually)
		// On Telium 1 (B2) VarId=0x0000 SerialNumber=0x0003C00F (Certificate)
		// On Telium 2 (B3) VarId=0x0119 SerialNumber=0x000104EC (Certificate)
		// Secret area identification from 00002130 to 3FFF2F3F
		//             (2 first bytes from 0000 to 3FFF)
		// On Tetra    (ISLERO) ASID=0x0010 SerialNumber=0x000000A6 (Certificate)
		// Secret area identification from 00002130 to FFFF2F3F
		//             (2 first bytes from 0000 to FFFF)
		// =====================================================================
		switch (iBoosterType)
		{
		case BOOSTER_1:
		case BOOSTER_2:
			// Could be VarId=0x0000 but NOT MANDATORY + 2&3 fixed + 1&0 user
			iAreaId = 0x00002130;
			break;
		case BOOSTER_3:
			// Could be VarId=0x0119 but NOT MANDATORY + 2&3 fixed + 1&0 user
			iAreaId = 0x01192130;
			break;
		case BOOSTER_3+1:
		// Could be ASID=0x0010 but NOT MANDATORY + 2&3 fixed + 1&0 user
		iAreaId=0x00102130;
		break;
		default:
			iAreaId = 0;
			break;
		}
	}

	return iAreaId;
}

//****************************************************************************
//         int PinEntry (const char *pcMsg1, const char *pcMsg2,
//                       const char *pcMsg1Idle, const char *pcMsg2Idle,
//                       byte ucTimeOut)
//  This function manages the pin entry.
//       SEC_PinEntryInit : To initialize the parameters for the secure
//                          Pin entry code
//       SEC_PinEntry : To manage the secure pincode entry
//  This function has parameters.
//    pcMsg1 (I-) : Message to display on first line
//    pcMsg2 (I-) : Message to display on second line
//    pcMsg1Idle (I-) : Idle message on first line
//    pcMsg2Idle (I-) : Idle message on second line
//    ucTimeOut (I-) : TimeOut (sec) Max 60 sec => Max supported by system
//  This function has return value.
//    >0 : Pin entry done
//     0 : Timeout/Cancel
//    <0 : Pin entry failed
//****************************************************************************

static int PinEntry(const char *pcMsg1, const char *pcMsg2,
		const char *pcMsg1Idle, const char *pcMsg2Idle, byte ucTimeOut)
{
	// Local variables
	// ***************
	T_GL_HWIDGET hScreen=NULL;
	doubleword uiEvent=0, uiEveWait;
	char tcPin[16+1];
	int iContinue=TRUE;
	bool bExitPin=FALSE;
	byte ucKey;
	int iPpd, iPinSize, iRet;
	T_SEC_ENTRYCONF xEntryCfg;

	// Prompt messages on terminal or PinPad
	// *************************************
	iPpd = IsPinOnPpd(); CHECK(iPpd>=0, lblKO);
	switch (iPpd)
	{
	case 0:                                                                                // *** Pin on terminal
		hScreen = GoalCreateScreen(gGoalGraphicLibInstance, txPin, NUMBER_OF_LINES(txPin), GL_ENCODING_UTF8);
		CHECK(hScreen!=NULL, lblKO);                                                       // Create screen and clear it
		iRet = GoalClrScreen(hScreen, GL_COLOR_TRANSPARENT, 0, false);
		CHECK(iRet>=0, lblKO);

		iRet = GoalDspLine(hScreen, 0, (char*) (pcMsg1+8), &txPin[0], 0, false);           // Amount message on terminal
		CHECK(iRet>=0, lblKO);
		iRet = GoalDspLine(hScreen, 1, (char*)pcMsg2, &txPin[1], 0, false);                // Pin input on terminal
		CHECK(iRet>=0, lblKO);
		iRet = GoalDspLine(hScreen, 2, "Enter your code\nfrom prying eyes", &txPin[2], 0, true);
		CHECK(iRet>=0, lblKO);                                                             // Warm message on terminal
		uiEvent=0;                                                                         // Wait for timeout
		break;
	case 1:                                                                                // *** Pin on pinpad with characters display
		GL_Dialog_Message(gGoalGraphicLibInstance, NULL, "Please Enter Pin\nOn Pin Pad", GL_ICON_WARNING, GL_BUTTON_NONE, 0);
		PPS_firstline();
		PPS_Display((char*) pcMsg1);                                                       // First message on pinpad
		PPS_newline();
		PPS_Display((char*) pcMsg2);                                                       // Second message on pinpad
		PPS_newline();                                                                     // Return to first line
		uiEvent=KEYBOARD;                                                                  // Wait for EFT keyboard to cancel pin entry on pinpad
		break;
	case 2:                                                                                // *** Pin on pinpad with graphical display
		GL_Dialog_Message(gGoalGraphicLibInstance, NULL, "Please Enter Pin\nOn Pin Pad", GL_ICON_WARNING, GL_BUTTON_NONE, 0);
		InitContext(PERIPH_PPR);                                                           // Switch to graphic display pinpad
		_DrawExtendedString(0, 22, (char*) pcMsg1, _OFF_, _MEDIUM_, _NORMALE_);            // First message on pinpad
		_DrawExtendedString(0, 35, (char*) pcMsg2, _OFF_, _MEDIUM_, _NORMALE_);            // Second message on pinpad
		PaintGraphics();
		uiEvent=KEYBOARD;                                                                  // Wait for EFT keyboard to cancel pin entry on pinpad
		break;
	default:
		break;
	}

	// Pin entry code initialization
	// *****************************
	xEntryCfg.ucEchoChar = ECHO_PIN;                                                       // '*' echo character
	xEntryCfg.ucMinDigits = 4;                                                             // Min pin entry 4 digits
	xEntryCfg.ucMaxDigits = 4;                                                             // Max pin entry 4 digits
	if (ucTimeOut < 60)
		xEntryCfg.iFirstCharTimeOut = ucTimeOut*1000;                                      // Wait for first digit < 60s
	else
		xEntryCfg.iFirstCharTimeOut = 60*1000;                                             // Wait for first digit 60s (Max supported)
	xEntryCfg.iInterCharTimeOut = 10*1000;                                                 // Wait for next digits 10s

	// Secure part relative to the Pin entry function
	iRet = SEC_PinEntryInit (&xEntryCfg, C_SEC_CIPHERING); CHECK(iRet==OK, lblKO);         // C_SEC_CIPHERING secure part chosen to cipher Pin entry on terminal or pinpad

	// Pin entry code management
	// *************************
	iPinSize=0;
	strcpy (tcPin, pcMsg2);

	while (!bExitPin)
	{
		uiEveWait=uiEvent; // Init ttestall (from SEC_PinEntry) with the right peripheral events
		iRet = SEC_PinEntry (&uiEveWait, &ucKey, &iContinue);

		if (iRet == OK)
		{
			if (ucKey == ECHO_PIN)
			{
				// Enter pin in progress
				// =====================
				if (iPinSize < MAX_PIN_CODE_SIZE)
				{
					tcPin[strlen(pcMsg2)+iPinSize] = ucKey;
					tcPin[strlen(pcMsg2)+iPinSize+1] = 0;
					switch (iPpd)
					{
					case 0:                                                                // *** Pin on terminal
						iRet = GoalDspLine(hScreen, 1, (char*)tcPin, &txPin[1], 0, true);
						CHECK(iRet>=0, lblKO);                                             // Display pin code entry
						break;
					case 1:                                                                // *** Pin on pinpad with characters display
						PPS_newline(); PPS_Display(tcPin); PPS_newline(); 				   // Display pin code entry on second line (to avoid using PPS_clearline())
						break;
					case 2:                                                                // *** Pin on pinpad with graphical display
						DrawExtendedString(0, 35, tcPin, _OFF_, _MEDIUM_, _NORMALE_);      // Display pin code entry
						break;
					default:
						break;
					}
					iPinSize++;
				}
			}
			else
			{
				// Pin confirmation
				// ================
				switch (ucKey)
				{
				case 0x00:                   // Response to stop pin entry by receiving an event
					if(uiEveWait & KEYBOARD) // Key event from terminal
					{
						GL_Dialog_Message(gGoalGraphicLibInstance, NULL, "EVENT RECEIVED\nKEY PRESSED", GL_ICON_INFORMATION, GL_BUTTON_NONE, 2*1000);
					}
					iRet=0;
					break;
				case 0x01:   iRet=0; break;
				case T_VAL:  iRet=1; break;  // Valid key from Pinpad or Terminal when enter pin => iContinue=FALSE if >= Min pin entry
				case T_ANN:  iRet=0; break;  // Cancel key from Pinpad or Terminal when enter pin => iContinue=FALSE
				case T_CORR:                 // Correction from Pinpad or Terminal when enter pin => iContinue=TRUE
					if (iPinSize != 0)
					{
						iPinSize--;
						tcPin[strlen(pcMsg2)+iPinSize] = ' ';
						tcPin[strlen(pcMsg2)+iPinSize+1] = 0;
						switch (iPpd)
						{
						case 0:                                                            // *** Pin on terminal
							iRet = GoalDspLine(hScreen, 1, (char*)tcPin, &txPin[1], 0, true);
							CHECK(iRet>=0, lblKO);                                         // Pin entry code correction
							break;
						case 1:                                                            // *** Pin on pinpad with characters display
							PPS_newline(); PPS_Display(tcPin); PPS_newline(); 			   // Pin entry code correction on second line (to avoid using PPS_clearline())
							break;
						case 2:                                                            // *** Pin on pinpad with graphical display
							DrawExtendedString(0, 35, tcPin, _OFF_, _MEDIUM_, _NORMALE_);  // Pin entry code correction
							break;
						default:
							break;
						}
					}
					break;
				default:             break;
				}
			}
		}
		else if (iRet == ERR_TIMEOUT)                                                      // Pin entry on pinpad and canceled by terminal
			bExitPin=TRUE;
		else                                                                               // Pin entry on pinpad already in progress
		{
			iRet=-1;
			bExitPin=TRUE;
		}

		// Pin exit process
		// ================
		if (iContinue  == FALSE)                                                           // The Pin entry is already stopped
		{
			ResetPeripherals(KEYBOARD | TSCREEN);                                          // Reset keyboard/touch FIFO for unexpected keys
			bExitPin=TRUE;                                                                 // exit immediately
		}
		else if (bExitPin == TRUE)                                                         // The Pin entry is not stopped
		{
			iContinue=FALSE;                                                               // To stop the Pin entry
			bExitPin=FALSE;                                                                // loop one more time before exit
		}
	}

	goto lblEnd;

	// Errors treatment
	// ****************
	lblKO:
	iRet=-1;                                                                               // Pin entry failed
	goto lblEnd;
	lblEnd:
	switch (iPpd)
	{
	case 0:                                                                                // *** Pin on terminal
		Telium_Ttestall(0, 1);                                                             // To consume the cancel key from Pin entry
		if (hScreen)
			GoalDestroyScreen(&hScreen);                                                   // Destroy screen
		break;
	case 1:                                                                                // *** Pin on pinpad with characters display
		PPS_firstline();
		PPS_Display((char*) pcMsg1Idle);                                                   // Idle message
		PPS_newline();
		PPS_Display((char*) pcMsg2Idle);                                                   // Second message on pinpad
		break;
	case 2:                                                                                // *** Pin on pinpad with graphical display
		_DrawExtendedString(0, 22, (char*) pcMsg1Idle, _OFF_, _MEDIUM_, _NORMALE_);        // Idle message
		_DrawExtendedString(0, 35, (char*) pcMsg2Idle, _OFF_, _MEDIUM_, _NORMALE_);
		PaintGraphics();
		InitContext(PERIPH_DISPLAY);                                                       // Switch to graphic display terminal
		break;
	default:
		break;
	}

	return iRet;
}

//****************************************************************************
//                          void SetConfig(void)
//  This function sets the configuration of the security component for the
//  current application.
//       SEC_SetConfig() : To configure the security component.
//  This function has no parameters.
//  This function has no return value.
//****************************************************************************

void SetConfig(void)
{
	// Local variables
	// ***************
	T_SEC_CONFIG txConfParts[C_NB_PARTS];
	int i, iNbParts, iDefItem;
	char *pcTitle;
	int iPpdBooster=0, iTermBooster=0, iRet;

	// Check Pinpad
	// ============
	iRet = DLLExist("SECURITY"); CHECK(iRet==TRUE, lblNoSecDll);              // Security DLL loaded?
	iRet = ChkPpdConfig(); CHECK(iRet>=0, lblEnd);                            // Pinpad problem? => check config from Manager
	CHECK(iRet!=0, lblNoPPad);                                                // No pinpad connected?

	// Get the configuration parts on the security component
	// *****************************************************
	iRet = SEC_GetConfig(&iNbParts, txConfParts); CHECK(iRet==OK, lblKO);     // Get configuration parts by default
	for (i=0; i<iNbParts; i++)
	{
		if ( memcmp (txConfParts[i].ptszBoosterPeripheral, IAPP_USB_NAME, strlen(IAPP_USB_NAME)) == 0)
			iPpdBooster = txConfParts[i].cBoosterType;                        // Pinpad booster type 1/2
		else
			iTermBooster = txConfParts[i].cBoosterType;                       // Terminal booster type 1/2/3/Islero
	}

	// Set the configuration parts on the security component
	// *****************************************************
	for (i=0; i<iNbParts; i++)
	{

		iRet = ChkPpdConfig();
		if (iRet > 0)                                                         // Pinpad declared in Manager and connected?
		{
			switch (txConfParts [i].SecureType)                               // Yes, Prompt for switching some secure parts
			{
			case C_SEC_PINCODE  : pcTitle = "PIN ENTRY ?";      break;        // Pin on terminal or pinpad?
			case C_SEC_CARD     : pcTitle = "CARD READER ?";    break;        // Card on terminal or pinpad?
			case C_SEC_CIPHERING: pcTitle = "DATA CIPHERING ?"; break;        // Ciphering on terminal or pinpad?
			default:              goto lblKO;
			}

			switch (txConfParts [i].SecureType)                               // Select the secure parts to switch
			{
			case C_SEC_PINCODE :
			case C_SEC_CARD :
#ifndef __TELIUM3__
			case C_SEC_CIPHERING :                                            // No secret area on pinpad for TETRA, ciphering always on terminal
#endif
				if (memcmp (txConfParts[i].ptszBoosterPeripheral, IAPP_USB_NAME, strlen(IAPP_USB_NAME)) == 0)
					iDefItem=1;
				else
					iDefItem=0;
				iRet = GL_Dialog_Choice(gGoalGraphicLibInstance, (const char*) pcTitle, tzMenuChoice, iDefItem, GL_BUTTON_DEFAULT, GL_KEY_0, GL_TIME_MINUTE);
				switch (iRet)
				{
				case 0:
					txConfParts[i].cBoosterType = (char) iTermBooster;        // Booster type 1/2/3
					txConfParts[i].ptszBoosterPeripheral = IAPP_DEF_NAME;     // DEF_NAME internal booster
					break;
				case 1:
					txConfParts[i].cBoosterType = (char) iPpdBooster;         // Booster type 1/2
					txConfParts[i].ptszBoosterPeripheral = IAPP_USB_NAME;     // USB_NAME external booster
					break;
				default:
					goto lblEnd;                                              // Exit on cancel/timeout
				}
				break;
				default:
					break;
			}
		}
	}

	iRet = SEC_SetConfig (iNbParts, txConfParts); CHECK(iRet==OK, lblKO);   // Set configuration parts
	GL_Dialog_Message(gGoalGraphicLibInstance, NULL, "Set Security\nConfiguration done", GL_ICON_INFORMATION, GL_BUTTON_VALID, 3*1000);

	goto lblEnd;

	// Errors treatment
	// ****************
	lblNoSecDll:                                                                  // Security DLL missing
	GL_Dialog_Message(gGoalGraphicLibInstance, NULL, "Missing Sec DLL", GL_ICON_WARNING, GL_BUTTON_VALID, 3*1000);
	goto lblEnd;
	lblNoPPad:
	GL_Dialog_Message(gGoalGraphicLibInstance, NULL, "No PinPad Connected", GL_ICON_ERROR, GL_BUTTON_VALID, 5*1000);
	goto lblEnd;
	lblKO:                                                                        // Security DLL error
	GL_Dialog_Message(gGoalGraphicLibInstance, NULL, "Processing Error", GL_ICON_ERROR, GL_BUTTON_VALID, 5*1000);
	goto lblEnd;
	lblEnd:
	return;
}

//****************************************************************************
//                          void GetConfig(void)
//  This function gets the configuration of the security component for the
//  current application.
//       SEC_GetConfig() : Configuration of the security component.
//       SEC_listSecureId() : List Ids of loaded schemes and secret areas
//                            in a secure part.
//  This function has no parameters.
//  This function has no return value.
//****************************************************************************

void GetConfig(void)
{
	// Local variables
	// ***************
	T_GL_HWIDGET hScreen=NULL;
	T_GL_HWIDGET hDocument=NULL;
	ST_PRN_LINE xLine;
	T_SEC_CONFIG txConfParts[C_NB_PARTS];
	int iNbParts, iNbId, tiList[10];
	T_SEC_LISTTLVKEY txKeyList[50];
	int iNbKey, i2;
	char cBooster;
	char tcDisplay[256];
	char tcPrint[2048];
	bool bArea;
	byte p; // Printer line index
	int i, i1, iRet;
	T_SEC_DATAKEY_ID xEraseKey;
	unsigned short Location = 1;
	byte tucChkSum[3];
	doubleword uiLen;

	// Check Pinpad
	// ============
	hScreen = GoalCreateScreen(gGoalGraphicLibInstance, txSec, NUMBER_OF_LINES(txSec), GL_ENCODING_UTF8);
	CHECK(hScreen!=NULL, lblKO);                                     // Create screen and clear it
	iRet = GoalClrScreen(hScreen, GL_COLOR_TRANSPARENT, KEY_VALID, false);
	CHECK(iRet>=0, lblKO);

	iRet = DLLExist("SECURITY"); CHECK(iRet==TRUE, lblNoSecDll);     // Security DLL loaded?
	iRet = ChkPpdConfig(); CHECK(iRet>=0, lblEnd);                   // Pinpad problem? => check config from Manager

	// Get the configuration of the security component
	// ===============================================
	iRet = SEC_GetConfig(&iNbParts, txConfParts); CHECK(iRet==OK, lblKO);
	sprintf(tcPrint, "Number of secure parts: %d\n", iNbParts);
	strcat(tcPrint, "     ----------------------------------\n");
	for (i=0; i<iNbParts; i++)
	{
		switch (txConfParts[i].SecureType)
		{
		case C_SEC_PINCODE:   strcpy(tcDisplay, "PIN ENTRY on ");   strcat(tcPrint, "PIN ENTRY on ");   break;
		case C_SEC_CARD:      strcat(tcDisplay, "CARD READER on "); strcat(tcPrint, "CARD READER on "); break;
		case C_SEC_CIPHERING: strcat(tcDisplay, "CIPHERING on ");   strcat(tcPrint, "CIPHERING on ");   break;
		default:              goto lblKO;
		}
		// Get device type Pinpad or Terminal
		if (memcmp (txConfParts[i].ptszBoosterPeripheral, IAPP_USB_NAME, strlen(IAPP_USB_NAME)) == 0)
		{
			strcat(tcDisplay, "PINPAD\n"); strcat(tcPrint, "PINPAD\n");
		}
		else
		{
			strcat(tcDisplay, "TERMINAL\n"); strcat(tcPrint, "TERMINAL\n");
		}

		if (txConfParts[i].SecureType == C_SEC_CIPHERING)
		{
			cBooster = txConfParts[i].cBoosterType;                  // Get booster type (B1, B2, B3, Islero)
			switch (cBooster)
			{
			case C_SEC_BL1: strcat(tcPrint, "- Booster Type=1\n"); break;
			case C_SEC_BL2: strcat(tcPrint, "- Booster Type=2 or 3\n"); break;
			default:        strcat(tcPrint, "- Booster Type=Islero\n"); break;
			}
		}
		else
			strcat(tcDisplay, "\n");
	}
	strcat(tcPrint, "     ----------------------------------\n");

	// Get the list of Ids of loaded schemes and secret areas in a secure part
	// =======================================================================
	memset (tiList, 0, sizeof(tiList));
	iNbId=5;                                                     // Max of secret zone
	iRet = SEC_listSecureId (C_SEC_CIPHERING, &iNbId, tiList); CHECK(iRet==OK, lblKO);
	if (iNbId==0)
		strcat(tcPrint, "Secret Area Id(s): 0\n");               // No secret area available
	else
		strcat(tcPrint, "Secret Area Id(s):\n");
	bArea=FALSE;
	for (i1=0; i1<iNbId; i1++)                                   // Get list of secret areas
	{
		if (tiList[i1] == AreaId())
		{
			sprintf(&tcPrint[strlen(tcPrint)], "%08x  => Key Id(s):\n", tiList[i1]);
			iRet = SEC_listTLVKeys (C_SEC_CIPHERING, AreaId(), sizeof(txKeyList),
					&iNbKey, txKeyList);
			if (iRet == OK)
			{
				for (i2=0; i2<iNbKey; i2++)                      // Get list of keys from my secret area
					sprintf(&tcPrint[strlen(tcPrint)], "                            %08x\n", txKeyList[i2].uiTLVKeyId);
			}
			else
				strcat(tcPrint, "                            Scheme missing\n");

			sprintf(&tcDisplay[strlen(tcDisplay)], "Secret Area = %08x\n", tiList[i1]);
			iRet = SEC_isSecretArea(C_SEC_CIPHERING, tiList[i1]);
			if(iRet == OK){

				xEraseKey.iSecretArea = AreaId();;
				xEraseKey.cAlgoType   = TLV_TYPE_KTDES;
				xEraseKey.usNumber    = 0; //This is what we are looking for
				xEraseKey.uiBankId    = BANK_ROOT_ID;

				iRet = SEC_KeyVerify (C_SEC_CIPHERING, &xEraseKey, tucChkSum, &uiLen);                                // Length checksum
				if (iRet != OK) {
					while(xEraseKey.usNumber < 768){
						xEraseKey.usNumber = Location;
						iRet = SEC_KeyVerify (C_SEC_CIPHERING, &xEraseKey, tucChkSum, &uiLen);
						if (iRet == OK) {
							iRet = Telium_Pprintf("   Key Loc %04d\n", xEraseKey.usNumber); CHECK(iRet>=0, lblKO);
							Location+=4;		//To skip 4 elements used by this key loc.
							continue;
						}
						Location++;
					}
					Location = 1;
				}
				iRet = Telium_Pprintf("\n\n"); CHECK(iRet>=0, lblKO);
			}

			bArea=TRUE;
		}
		else
			sprintf(&tcPrint[strlen(tcPrint)], "%08x\n", tiList[i1]);
	}
	if(!bArea)
		strcat(tcDisplay, "\n");

	strcat(tcPrint, "\n\n\n\n");

	// Display configuration
	// =====================
	iRet = GoalDspMultiLine(hScreen, 0, tcDisplay, strlen(tcDisplay), txSec, NUMBER_OF_LINES(txSec), 0, true);
	CHECK(iRet>=0, lblKO);                                           // Show info security

	// Print configuration
	// ===================
	p=0;
	hDocument = GoalCreateDocument(gGoalGraphicLibInstance, GL_ENCODING_UTF8);         // Create document
	CHECK(hDocument!=NULL, lblKO);

	xLine = xPrinter;                                                // Build document (Security, config info)
	xLine.eTextAlign = GL_ALIGN_CENTER;
	xLine.bReverse = TRUE;
	xLine.xMargin.usBottom = PIXEL_BOTTOM;
	xLine.xFont.eScale = GL_SCALE_XXLARGE;
	iRet = GoalPrnLine(hDocument, p++, "Security Config", &xLine);
	CHECK(iRet>=0, lblKO);

	xLine = xPrinter;
	iRet = GoalPrnLine(hDocument, p++, tcPrint, &xLine);
	CHECK(iRet>=0, lblKO);

	iRet = GoalPrnDocument(hDocument);                               // Print document
	CHECK(iRet>=0, lblKO);

	GoalGetKey(hScreen, gGoalGraphicLibInstance, true, 30*1000, true);                 // Wait for key pressed/touched (shortcut)

	goto lblEnd;

	// Errors treatment
	// ****************
	lblNoSecDll:                                                         // Security DLL missing
	GL_Dialog_Message(gGoalGraphicLibInstance, NULL, "Missing Sec DLL", GL_ICON_WARNING, GL_BUTTON_VALID, 3*1000);
	goto lblEnd;
	lblKO:                                                               // None-classified low level error
	GL_Dialog_Message(gGoalGraphicLibInstance, NULL, "Processing Error", GL_ICON_ERROR, GL_BUTTON_VALID, 5*1000);
	goto lblEnd;
	lblEnd:
	if (hScreen)
		GoalDestroyScreen(&hScreen);                                 // Destroy screen
	if (hDocument)
		GoalDestroyDocument(&hDocument);                             // Destroy document
}

//****************************************************************************
//                          void CreateSecret(void)
//  This function creates a secret area and loads the Root key.
//       SEC_CreateSecretArea() : To create a secret area
//       SEC_LoadKey() : To load a key in secret area
//  This function has no parameters.
//  This function has no return value.
//****************************************************************************

void CreateSecret(void)
{
	// Local variables
	// ***************
	char tcDisplay[50+1];
	byte tucChkSum[3];
	T_SEC_DATAKEY_ID xRootKey;
	doubleword uiLen;
	int iRet;

	// Check Pinpad
	// ************
	iRet = DLLExist("SECURITY"); CHECK(iRet==TRUE, lblNoSecDll);  // Security DLL loaded?
	iRet = ChkPpdConfig(); CHECK(iRet>=0, lblEnd);                // Pinpad problem? => check config from Manager

	xRootKey.iSecretArea = AreaId();                              // Secret area identification
	xRootKey.cAlgoType   = TLV_TYPE_KTDES;                        // This ROOT key is a TDES Key
	xRootKey.usNumber    = ROOT_KEY_LOC;                          // ROOT key location inside the secret area
	xRootKey.uiBankId    = BANK_ROOT_ID;                          // Bank id related to this ROOT key

	// Create Secret Area ONLY for Booster 1, does not do anything for Booster 2&3
	// ***************************************************************************
#ifndef __TELIUM3__
	iRet = SEC_CreateSecretArea (C_SEC_CIPHERING,                 // C_SEC_CIPHERING secure part chosen to create secret area in terminal or pinpad
			(SEG_ID) AreaId(),               // Secret area identification
			SerialNb(),                      // Serial number to protect secret area
			VarId());                        // Var Id to protect secret area
	CHECK(iRet==OK, lblSecKO);

	// Load Root Key for Booster 1
	// Create Secret Area and load Root Key for Booster 2&3 (CARD_NB and VAR_ID are retrieved automatically from the certificate attached to the binary)
	// *************************************************************************************************************************************************

	iRet = SEC_LoadKey (C_SEC_CIPHERING,                          // C_SEC_CIPHERING secure part chosen to load key in terminal or pinpad
			NULL,
			&xRootKey,                                // ROOT key parameters
			(byte*)zRootKeyPlainText,                 // The ROOT key value
			CIPHERING_KEY);                           // ROOT key usage : to cipher PIN key or MAC key
	CHECK(iRet==OK, lblSecKO);

#endif

	// Root Key CheckSum
	// *****************
	iRet = SEC_KeyVerify (C_SEC_CIPHERING,                        // C_SEC_CIPHERING secure part chosen to check key in terminal or pinpad
			&xRootKey,                              // ROOT key parameters
			tucChkSum,                              // ROOT key checksum
			&uiLen);                                // Length checksum
	CHECK(iRet==OK, lblSecKO);

	sprintf(tcDisplay, "Create Area Done\nCheckSum: %02X%02X%02X", tucChkSum[0], tucChkSum[1], tucChkSum[2]);
	GL_Dialog_Message(gGoalGraphicLibInstance, NULL, tcDisplay, GL_ICON_INFORMATION, GL_BUTTON_VALID, 5*1000);

	goto lblEnd;

	// Errors treatment
	// ****************
	lblNoSecDll:                                                      // Security DLL missing
	GL_Dialog_Message(gGoalGraphicLibInstance, NULL, "Missing Sec DLL", GL_ICON_WARNING, GL_BUTTON_VALID, 3*1000);
	goto lblEnd;
	lblSecKO:                                                         // Security DLL error
#ifdef __TELIUM3__
	if (iRet == STS_ERR_SYS_SCH_NOT_FOUND)
		GL_Dialog_Message(gGoalGraphicLibInstance, NULL, "KeyInject Tool\nRequired", GL_ICON_ERROR, GL_BUTTON_VALID, 5*1000);
	else
#endif
		GL_Dialog_Message(gGoalGraphicLibInstance, NULL, SEC_ErrorMsg(iRet), GL_ICON_ERROR, GL_BUTTON_VALID, 5*1000);
	goto lblEnd;
	lblEnd:
	return;
}

//****************************************************************************
//                          void FreeSecret(void)
//  This function is used to kill the secret area.
//       SEC_FreeSecret() : To free a secret data or the whole secret area.
//  This function has no parameters.
//  This function has no return value.
//****************************************************************************

void FreeSecret(void)
{
	// Local variables
	// ***************
	T_SEC_DATAKEY_ID xEraseKey;
	int iRet;

	// Check Pinpad
	// ************
	iRet = DLLExist("SECURITY"); CHECK(iRet==TRUE, lblNoSecDll); // Security DLL loaded?
	iRet = ChkPpdConfig(); CHECK(iRet>=0, lblEnd);               // Pinpad problem? => check config from Manager

#ifdef __TELIUM2__
	// Erase Secret Area
	// *****************
	xEraseKey.iSecretArea = AreaId();                            // Secret area identification
	xEraseKey.cAlgoType = 0;
	xEraseKey.usNumber = 0;
	xEraseKey.uiBankId = 0;
	iRet = SEC_FreeSecret (C_SEC_CIPHERING,                      // C_SEC_CIPHERING secure part to erase secret area in terminal or pinpad
			&xEraseKey);                          // Secret area to erase
	CHECK(iRet==OK, lblSecKO);
#endif

	GL_Dialog_Message(gGoalGraphicLibInstance, NULL, "Free Secret Area Done", GL_ICON_INFORMATION, GL_BUTTON_VALID, 3*1000);

	goto lblEnd;

	// Errors treatment
	// ****************
	lblNoSecDll:                                                     // Security DLL missing
	GL_Dialog_Message(gGoalGraphicLibInstance, NULL, "Missing Sec DLL", GL_ICON_WARNING, GL_BUTTON_VALID, 3*1000);
	goto lblEnd;
	lblSecKO:                                                        // Security DLL error
	GL_Dialog_Message(gGoalGraphicLibInstance, NULL, SEC_ErrorMsg(iRet), GL_ICON_ERROR, GL_BUTTON_VALID, 5*1000);
	goto lblEnd;
	lblEnd:
	return;
}

//****************************************************************************
//                   void LoadIso9564Key(void)
//  This function loads Iso9564 pin key inside the secret area by using the
//  Root key.
//       SEC_LoadKey() : To load a key in secret area
//  This function has no parameters.
//  This function has no return value.
//****************************************************************************

void LoadIso9564Key(void)
{
	// Local variables
	// ***************
	char tcDisplay[50+1];
	byte tucChkSum[3];
	T_SEC_DATAKEY_ID xRootKey, xPinKey;
	doubleword uiLen;
	int iRet;

	// Check Pinpad
	// ============
	iRet = DLLExist("SECURITY"); CHECK(iRet==TRUE, lblNoSecDll); // Security DLL loaded?
	iRet = ChkPpdConfig(); CHECK(iRet>=0, lblEnd);               // Pinpad problem? => check config from Manager

	// Load Pin Key using Root Key
	// ***************************
	xRootKey.iSecretArea = AreaId();                             // Secret area identification
	xRootKey.cAlgoType   = TLV_TYPE_KTDES;                       // This ROOT key is a TDES Key
	xRootKey.usNumber    = ROOT_KEY_LOC;                         // ROOT key location inside the secret area
	xRootKey.uiBankId    = BANK_ROOT_ID;                         // Bank id related to this ROOT key

	xPinKey.iSecretArea = AreaId();                              // Secret area identification
	xPinKey.cAlgoType   = TLV_TYPE_KTDES;                        // This PIN key is a TDES Key
	xPinKey.usNumber    = ISO9564PIN_KEY_LOC;                    // PIN key location inside the secret area
	xPinKey.uiBankId    = BANK_ISO9564_ID;                       // Bank id related to this PIN key

	iRet = SEC_LoadKey (C_SEC_CIPHERING,                         // C_SEC_CIPHERING secure part to load key in terminal or pinpad
			&xRootKey,                               // ROOT key parameters
			&xPinKey,                                // PIN key parameters
			(byte*)zIso9564PinKey,                   // The ISO9564 PIN key value
			CIPHERING_PIN);                          // PIN key usage : to cipher PIN entry
	//CHECK(iRet==OK, lblSecKO);

	// Pin Key CheckSum
	// ****************
	iRet = SEC_KeyVerify (C_SEC_CIPHERING,                       // C_SEC_CIPHERING secure part to check key in terminal or pinpad
			&xPinKey,                              // PIN key parameters
			tucChkSum,                             // PIN key checksum
			&uiLen);                               // Length checksum
	CHECK(iRet==OK, lblSecKO);

	sprintf(tcDisplay, "Load Pin Key Done\nCheckSum: %02X%02X%02X", tucChkSum[0], tucChkSum[1], tucChkSum[2]);
	GL_Dialog_Message(gGoalGraphicLibInstance, NULL, tcDisplay, GL_ICON_INFORMATION, GL_BUTTON_VALID, 5*1000);

	goto lblEnd;

	// Errors treatment
	// ****************
	lblNoSecDll:                                                     // Security DLL missing
	GL_Dialog_Message(gGoalGraphicLibInstance, NULL, "Missing Sec DLL", GL_ICON_WARNING, GL_BUTTON_VALID, 3*1000);
	goto lblEnd;
	lblSecKO:                                                        // Security DLL error
	GL_Dialog_Message(gGoalGraphicLibInstance, NULL, SEC_ErrorMsg(iRet), GL_ICON_ERROR, GL_BUTTON_VALID, 5*1000);
	goto lblEnd;
	lblEnd:
	return;
}

//****************************************************************************
//                          void LoadMacKey(void)
//  This function loads the Mac key inside the secret area by using the Root key.
//       SEC_LoadKey() : To load a key in secret area
//  This function has no parameters.
//  This function has no return value.
//****************************************************************************

void LoadMacKey(void)
{
	// Local variables
	// ***************
	char tcDisplay[50+1];
	byte tucChkSum[3];
	T_SEC_DATAKEY_ID xRootKey, xMacKey;
	doubleword uiLen;
	int iRet;

	// Check Pinpad
	// ************
	iRet = DLLExist("SECURITY"); CHECK(iRet==TRUE, lblNoSecDll); // Security DLL loaded?
	iRet = ChkPpdConfig(); CHECK(iRet>=0, lblEnd);               // Pinpad problem? => check config from Manager

	// Load Mac Key using Root Key
	// ***************************
	xRootKey.iSecretArea = AreaId();                             // Secret area identification
	xRootKey.cAlgoType   = TLV_TYPE_KTDES;                       // This ROOT key is a TDES Key
	xRootKey.usNumber    = ROOT_KEY_LOC;                         // ROOT key location inside the secret area
	xRootKey.uiBankId    = BANK_ROOT_ID;                         // Bank id related to this ROOT key

	xMacKey.iSecretArea = AreaId();                              // Secret area identification
	xMacKey.cAlgoType   = TLV_TYPE_KDES;                         // This MAC key is a DES Key
	xMacKey.usNumber    = MAC_KEY_LOC;                           // MAC key location inside the secret area
	xMacKey.uiBankId    = BANK_MAC_ID;                           // Bank id related to this MAC key

	iRet = SEC_LoadKey (C_SEC_CIPHERING,                         // C_SEC_CIPHERING secure part chosen to load key in terminal or pinpad
			&xRootKey,                               // ROOT key parameters
			&xMacKey,                                // MAC key parameters
			(byte*)zMacKey,                          // The MAC key value
			CIPHERING_DATA);                         // MAC key usage : to cipher Data Message
	CHECK(iRet==OK, lblSecKO);

	// MAC Key CheckSum
	// ****************
	iRet = SEC_KeyVerify (C_SEC_CIPHERING,                       // C_SEC_CIPHERING secure part chosen to check key in terminal or pinpad
			&xMacKey,                              // MAC key parameters
			tucChkSum,                             // MAC key checksum
			&uiLen);                               // Length checksum
	CHECK(iRet==OK, lblSecKO);

	sprintf(tcDisplay, "Load Mac Key Done\nCheckSum: %02X%02X%02X", tucChkSum[0], tucChkSum[1], tucChkSum[2]);
	GL_Dialog_Message(gGoalGraphicLibInstance, NULL, tcDisplay, GL_ICON_INFORMATION, GL_BUTTON_VALID, 5*1000);

	goto lblEnd;

	// Errors treatment
	// ****************
	lblNoSecDll:                                                     // Security DLL missing
	GL_Dialog_Message(gGoalGraphicLibInstance, NULL, "Missing Sec DLL", GL_ICON_WARNING, GL_BUTTON_VALID, 3*1000);
	goto lblEnd;
	lblSecKO:                                                        // Security DLL error
	GL_Dialog_Message(gGoalGraphicLibInstance, NULL, SEC_ErrorMsg(iRet), GL_ICON_ERROR, GL_BUTTON_VALID, 5*1000);
	goto lblEnd;
	lblEnd:
	return;
}

//****************************************************************************
//        int Iso9564PinCiphering(const byte *pucAcc, byte *pucBlk)
//  This function enciphers the Pin entry by using the PIN key loaded in the
//  secret area to calculate the Pinblock.
//       SEC_Iso9564() : ISO 9564 Pin ciphering  (ANSI X9.8)
//  This function has parameters.
//    pucAcc (I-) : Account number
//    pucBlk (-O) : Pinblock
//  This function has return value.
//    =0 : Done
//    !0 : Failed
//****************************************************************************

static int Iso9564PinCiphering(const byte *pucAcc, byte *pucBlk)
{
	// Local variables
	// ***************
	doubleword uiLengthOut;
	T_SEC_DATAKEY_ID xKey;
	int iRet;

	// ISO9564 Pin ciphering
	// *********************
	xKey.iSecretArea = AreaId();                  // Secret area identification
	xKey.cAlgoType = TLV_TYPE_KTDES;              // PIN key is a TDES Key
	xKey.usNumber = ISO9564PIN_KEY_LOC;           // PIN key location inside the secret area
	xKey.uiBankId = BANK_ISO9564_ID;              // Bank id related to this PIN key

	iRet = SEC_Iso9564 (C_SEC_CIPHERING,          // C_SEC_CIPHERING secure part chosen to cipher PIN in terminal or pinpad
			&xKey,                    // PIN key parameters
			ISO9564_F0_TDES,          // Ciphering format
			(byte*)pucAcc,            // Account number (last 12 digits without Luhn)
			pucBlk,                   // Pinblock result
			&uiLengthOut);

	return iRet;
}

//****************************************************************************
//                     void Iso9564PinEntry(void)
//  This function manages the Pin entry (online) to get the Pinblock.
//  This function has no parameters.
//  This function has no return value.
//****************************************************************************

void Iso9564PinEntry(void)
{
	// Local variables
	// ***************
	T_GL_HWIDGET hScreen=NULL;
	T_GL_HWIDGET hDocument=NULL;
	ST_PRN_LINE xLine;
	char tcDisplay[50+1];
	char tcPrint[50+1];
	char *pcMsg1, *pcMsg2, *pcMsg1Idle, *pcMsg2Idle;
	byte tucAcc[8];
	byte tucBlk[8];
	byte p; // Printer line index
	int i, iRet;

	// Check Pinpad
	// ************
	iRet = DLLExist("SECURITY"); CHECK(iRet==TRUE, lblNoSecDll);       // Security DLL loaded?
	iRet = ChkPpdConfig(); CHECK(iRet>=0, lblEnd);                     // Pinpad problem? => check config from Manager

	// Enter PIN
	// *********
	pcMsg1 = "        1000 EUR";
	pcMsg2 = "PIN: ";
	pcMsg1Idle = "    WELCOME     ";
	pcMsg2Idle = "                ";
	iRet = PinEntry (pcMsg1, pcMsg2, pcMsg1Idle, pcMsg2Idle, 60);      // Pin entry with timeout 90sec
	CHECK(iRet!=0, lblEnd); CHECK(iRet>=0, lblKO);

	// PIN ciphering to get pinblock
	// *****************************
	memcpy(tucAcc, "\x00\x00\x78\x90\x12\x34\x56\x74", 8); // 12 last digit (without luhn) + left padding 0
	iRet = Iso9564PinCiphering(tucAcc, tucBlk); CHECK(iRet==OK, lblSecKO);

	// Display pinblock
	// ****************
	hScreen = GoalCreateScreen(gGoalGraphicLibInstance, txResult, NUMBER_OF_LINES(txResult), GL_ENCODING_UTF8);
	CHECK(hScreen!=NULL, lblKO);                                       // Create new screen and clear it
	iRet = GoalClrScreen(hScreen, GL_COLOR_TRANSPARENT, KEY_VALID, false);
	CHECK(iRet>=0, lblKO);

	iRet = GoalDspLine(hScreen, 0, "Pin Block Result", &txResult[0], 0, false);
	CHECK(iRet>=0, lblKO);
	memset(tcDisplay, 0, sizeof(tcDisplay));
	for (i=0; i<8; i++)
		sprintf(&tcDisplay[strlen(tcDisplay)], "%02X", tucBlk[i]);

	iRet = GoalDspLine(hScreen, 1, tcDisplay, &txResult[1], 0, true);  // Show pinblock
	CHECK(iRet>=0, lblKO);

	// Print pinblock
	// **************
	p=0;
	hDocument = GoalCreateDocument(gGoalGraphicLibInstance, GL_ENCODING_UTF8);           // Create document
	CHECK(hDocument!=NULL, lblKO);

	xLine = xPrinter;                                                  // Build document (Demo, account# info, Pin key, Pinblock)
	xLine.eTextAlign = GL_ALIGN_CENTER;
	xLine.bReverse = TRUE;
	xLine.xMargin.usBottom = PIXEL_BOTTOM;
	xLine.xFont.eScale = GL_SCALE_XXLARGE;
	iRet = GoalPrnLine(hDocument, p++, "Pin Entry Demo", &xLine);
	CHECK(iRet>=0, lblKO);

	xLine = xPrinter;
	iRet = GoalPrnLine(hDocument, p++, "Account Number:\n"             // Account number to print
			"1234567890123456744", &xLine);
	CHECK(iRet>=0, lblKO);
	iRet = GoalPrnLine(hDocument, p++,"Pin Key Checksum Value:\n"      // Pin KCV to print
			"F7 53 C2", &xLine);
	CHECK(iRet>=0, lblKO);
	iRet = GoalPrnLine(hDocument, p++, "Pinblock:", &xLine);
	CHECK(iRet>=0, lblKO);
	memset(tcPrint, 0, sizeof(tcPrint));
	for (i=0; i<8; i++)
		sprintf (&tcPrint[strlen(tcPrint)], "%02X ", tucBlk[i]);
	iRet = GoalPrnLine(hDocument, p++, tcPrint, &xLine);               // Pinblock to print
	CHECK(iRet>=0, lblKO);
	iRet = GoalPrnLine(hDocument, p++, "\n\n\n\n", &xLine);
	CHECK(iRet>=0, lblKO);

	iRet = GoalPrnDocument(hDocument);                                 // Print document
	CHECK(iRet>=0, lblKO);

	GoalGetKey(hScreen, gGoalGraphicLibInstance, true, 30*1000, true);                   // Wait for key pressed/touched (shortcut)

	goto lblEnd;

	// Errors treatment
	// ****************
	lblNoSecDll:                                                           // Security DLL missing
	GL_Dialog_Message(gGoalGraphicLibInstance, NULL, "Missing Sec DLL", GL_ICON_WARNING, GL_BUTTON_VALID, 3*1000);
	goto lblEnd;
	lblSecKO:                                                              // Security DLL error
	GL_Dialog_Message(gGoalGraphicLibInstance, NULL, SEC_ErrorMsg(iRet), GL_ICON_ERROR, GL_BUTTON_VALID, 5*1000);
	goto lblEnd;
	lblKO:                                                                 // None-classified low level error
	GL_Dialog_Message(gGoalGraphicLibInstance, NULL, "Processing Error", GL_ICON_ERROR, GL_BUTTON_VALID, 5*1000);
	goto lblEnd;
	lblEnd:
	if (hScreen)
		GoalDestroyScreen(&hScreen);                                   // Destroy screen

	if (hDocument)
		GoalDestroyDocument(&hDocument);                               // Destroy document
}

//****************************************************************************
//                       void MacCalculation(void)
//  This function manages the Mac calculation on a message.
//  This function has no parameters.
//  This function has no return value.
//****************************************************************************

void MacCalculation(void)
{
	// Local variables
	// ***************
	T_GL_HWIDGET hScreen=NULL;
	T_GL_HWIDGET hDocument=NULL;
	ST_PRN_LINE xLine;
	char tcDisplay[50+1];
	char tcPrint[50+1];
	T_SEC_DATAKEY_ID xMacKey;
	const byte tucMsg[] = { 0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,
			0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22,
			0x33,0x33,0x33,0x33,0x33,0x33,0x33,0x33 };
	byte tucMac[8];
	byte p; // Printer line index
	int i, iRet;

	hScreen = GoalCreateScreen(gGoalGraphicLibInstance, txResult, NUMBER_OF_LINES(txResult), GL_ENCODING_UTF8);
	CHECK(hScreen!=NULL, lblKO);                                      // Create new screen and clear it
	iRet = GoalClrScreen(hScreen, GL_COLOR_TRANSPARENT, KEY_VALID, false);
	CHECK(iRet>=0, lblKO);

	// Check Pinpad
	// ************
	iRet = DLLExist("SECURITY"); CHECK(iRet==TRUE, lblNoSecDll);      // Security DLL loaded?
	iRet = ChkPpdConfig(); CHECK(iRet>=0, lblEnd);                    // Pinpad problem? => check config from Manager

	iRet = GoalDspLine(hScreen, 0, "M.A.C Compute", &txResult[0], 0, true);
	CHECK(iRet>=0, lblKO);                                            // Compute in progress

	// Mac calculation
	// ***************
	xMacKey.iSecretArea = AreaId();                                   // Secret area identification
	xMacKey.cAlgoType = TLV_TYPE_KDES;                                // MAC key is a DES Key
	xMacKey.usNumber = MAC_KEY_LOC;                                   // MAC key location inside the secret area
	xMacKey.uiBankId = BANK_MAC_ID;                                   // Bank id related to this MAC key

	iRet = SEC_ComputeMAC (&xMacKey,                                  // MAC key parameters
			(byte*)tucMsg,                             // Message to compute
			sizeof(tucMsg),
			NULL,
			tucMac);                                   // MAC result
	CHECK(iRet==0, lblSecKO)

	// Display Mac
	// ***********
	iRet = GoalDspLine(hScreen, 0, "M.A.C Result", &txResult[0], 0, false);
	CHECK(iRet>=0, lblKO);

	memset(tcDisplay, 0, sizeof(tcDisplay));
	for (i=0; i<8; i++)
		sprintf(&tcDisplay[strlen(tcDisplay)], "%02X", tucMac[i]);

	iRet = GoalDspLine(hScreen, 1, tcDisplay, &txResult[1], 0, true); // Show MAC
	CHECK(iRet>=0, lblKO);

	// Print Mac
	// *********
	p=0;
	hDocument = GoalCreateDocument(gGoalGraphicLibInstance, GL_ENCODING_UTF8);          // Create document
	CHECK(hDocument!=NULL, lblKO);

	xLine = xPrinter;                                                 // Build document (Demo, data message, Mac key, Mac result)
	xLine.eTextAlign = GL_ALIGN_CENTER;
	xLine.bReverse = TRUE;
	xLine.xMargin.usBottom = PIXEL_BOTTOM;
	xLine.xFont.eScale = GL_SCALE_XXLARGE;
	iRet = GoalPrnLine(hDocument, p++, "MAC Demo", &xLine);
	CHECK(iRet>=0, lblKO);

	xLine = xPrinter;
	iRet = GoalPrnLine(hDocument, p++, "Data message:\n"              // Data message to print
			"11 11 11 11 11 11 11 11\n"
			"22 22 22 22 22 22 22 22\n"
			"33 33 33 33 33 33 33 33", &xLine);
	CHECK(iRet>=0, lblKO);
	iRet = GoalPrnLine(hDocument, p++, "MAC Key Checksum Value:\n"    // MAC KCV to print
			"04 6C 4B", &xLine);
	CHECK(iRet>=0, lblKO);
	iRet = GoalPrnLine(hDocument, p++, "MAC calculation:", &xLine);
	CHECK(iRet>=0, lblKO);
	memset(tcPrint, 0, sizeof(tcPrint));
	for (i=0; i<8; i++)
		sprintf (&tcPrint[strlen(tcPrint)], "%02X ", tucMac[i]);
	iRet = GoalPrnLine(hDocument, p++, tcPrint, &xLine);              // MAC calculation to print
	CHECK(iRet>=0, lblKO);
	iRet = GoalPrnLine(hDocument, p++, "\n\n\n\n", &xLine);
	CHECK(iRet>=0, lblKO);

	iRet = GoalPrnDocument(hDocument);                                // Print document
	CHECK(iRet>=0, lblKO);

	GoalGetKey(hScreen, gGoalGraphicLibInstance, true, 30*1000, true);                  // Wait for key pressed/touched (shortcut)

	goto lblEnd;

	// Errors treatment
	// ****************
	lblNoSecDll:                                                          // Security DLL missing
	GL_Dialog_Message(gGoalGraphicLibInstance, NULL, "Missing Sec DLL", GL_ICON_WARNING, GL_BUTTON_VALID, 3*1000);
	goto lblEnd;
	lblSecKO:                                                             // Security DLL error
	GL_Dialog_Message(gGoalGraphicLibInstance, NULL, SEC_ErrorMsg(iRet), GL_ICON_ERROR, GL_BUTTON_VALID, 5*1000);
	goto lblEnd;
	lblKO:                                                                // None-classified low level error
	GL_Dialog_Message(gGoalGraphicLibInstance, NULL, "Processing Error", GL_ICON_ERROR, GL_BUTTON_VALID, 5*1000);
	goto lblEnd;
	lblEnd:
	if (hScreen)
		GoalDestroyScreen(&hScreen);                                  // Destroy screen

	if (hDocument)
		GoalDestroyDocument(&hDocument);                              // Destroy document
}



void LoadKeysAuto(void){
	GetConfig();
	SetConfig();
	FreeSecret();
	CreateSecret();
	LoadIso9564Key();
	LoadMacKey();

}











