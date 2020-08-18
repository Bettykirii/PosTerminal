
#include "sdk_tplus.h"

#include "Menu.h"
#include "Training.h"
#include "globals.h"


//! \brief Ask for an amount.
//! \param[out] amount Entered amount.
//! \return \a TRUE if the amount is entered, \a FALSE if not (cancelled ...).
int amount() {
	int result = 0,ret = 0;
	const char entryMask[] = "/d/d/d,/d/d/d,/d/d/D./D/D";
	char currencyLabel[3 + 1] ;
	char amountString[32 + 1];

	memset(amountString, 0, sizeof(amountString));

	strcat(currencyLabel, "KES");

	result = (GL_Dialog_Amount(gGoalGraphicLibInstance, NULL, "Amount:", entryMask, amountString, sizeof(amountString) - 1, currencyLabel, GL_ALIGN_RIGHT, 2 * GL_TIME_MINUTE) == GL_KEY_VALID);
	if (result) {
		mapPut(traAmt, amountString, strlen(amountString));
	}

	lblKO:
	return 0;


	return result;
}
