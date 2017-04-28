
//#include <SoftwareSerial.h>

#include "LiquidCrystal_PCF8574.h"
#include "LCD_Menu.h"

extern LiquidCrystal_PCF8574 lcd;
extern void debug(char *sFmt, ...);

LCD_Menu::LCD_Menu(uint8_t rows, uint8_t columns) {
  _menu_item_first_visible = 1;
  _menu_items_count = 0;
  _menu_item_selected = 0;
  _lcd_columns = columns;
  _lcd_rows = rows;
  _edit_mode = false;
}

void LCD_Menu::setMenuItems(menu_item* menuitems, uint8_t number_of_items) {
  _menuitems = menuitems;
  _menu_items_count = number_of_items;
  //debug("set count: %d\n", _menu_items_count);
}

void LCD_Menu::updateLCD() {  
  int row, menuitem = _menu_item_first_visible-1;
  lcd.clear();
  for (row = 0; row < _lcd_rows; row++) {
    lcd.setCursor(0,row);
    lcd.print(_menuitems[menuitem].text);
    lcd.setCursor(_menuitems[menuitem].datacolumn, row);
    lcd.print(_getMenuItemValue(menuitem+1));
    menuitem++;
  }
}

void LCD_Menu::uiUp() {
  if (!_edit_mode) {
    _moveMenu(false);
    return;
  }

  // edit mode
  _changeMenuItemValue(1);
  updateSelectedMenuItemValue();
}

void LCD_Menu::uiDown() {
  if (!_edit_mode) {
    _moveMenu(true);
    return;
  }  

  // edit mode
  _changeMenuItemValue(2);
  updateSelectedMenuItemValue();
}

void LCD_Menu::uiSelect(bool longPress) {
  //debug("_uiSelect %d\n", longPress);
  if (!_edit_mode) {
    _setEditMode(true);
    return;
  } 
  
  // edit mode is already active

  // finish editing?
  if (longPress) {
    _setEditMode(false);
    return;
  }

  // move to next menu item
  // increment menu item and check if still in range
  if (++_menu_item_selected > (_menu_items_count)) {
    _menu_item_selected = 1;
    //debug("selected reset: %d\n", _menu_item_selected, _menu_items_count);
  }

  //debug("selected: %d of %d\n", _menu_item_selected, _menu_items_count);
  
  // selected menu item visible?
  if (_menu_item_selected < _menu_item_first_visible) {
    _menu_item_first_visible = _menu_item_selected;
    //debug("visible: %d selected: %d\n", _menu_item_first_visible, _menu_item_selected);
    updateLCD();
  }
  if (_menu_item_selected >= (_menu_item_first_visible + _lcd_rows) ) {
    _moveMenu(true);
  }
  _updateCursor();
}

bool LCD_Menu::getEditMode() {
  return _edit_mode;
}

void LCD_Menu::updateSelectedMenuItemValue() {
  lcd.setCursor(_menuitems[_menu_item_selected-1].datacolumn, (_menu_item_selected - _menu_item_first_visible));
  lcd.print("     ");
  lcd.setCursor(_menuitems[_menu_item_selected-1].datacolumn, (_menu_item_selected - _menu_item_first_visible));
  lcd.print(_getMenuItemValue(_menu_item_selected));
  lcd.setCursor(_menuitems[_menu_item_selected-1].datacolumn, (_menu_item_selected - _menu_item_first_visible));
}

void LCD_Menu::_moveMenu(bool down) {
  //debug("First Visible: %d (%d, %d)\n", _menu_item_first_visible, _menu_items_count, _lcd_rows);
  if (down) {
    if (_menu_item_first_visible <= (_menu_items_count - _lcd_rows) ) {
      _menu_item_first_visible++;
      updateLCD();
    }
  } else {  // up  
    if (_menu_item_first_visible > 1) {
      _menu_item_first_visible--;
      updateLCD();
    }
  }
}

void LCD_Menu::_setEditMode(bool enable) {
  //debug("_setEditMode %d\n", enable);
  // do we need to change edit mode?
  if (enable == getEditMode()) return;

  // edit on
  if (enable) {
    _edit_mode = true;
    _menu_item_selected = _menu_item_first_visible;
  } else {  // edit off
    _edit_mode = false;
    _menu_item_selected = 0;
  }
  _updateCursor();
}

void LCD_Menu::_updateCursor() {
  lcd.setCursor(_menuitems[_menu_item_selected-1].datacolumn, (_menu_item_selected - _menu_item_first_visible) );
  if (_edit_mode) {
    lcd.blink();
  } else {
    lcd.noBlink();
  }
}

int LCD_Menu::getSelectedMenuItem()
{
  return(_menu_item_selected);    // 0 = none
}

void LCD_Menu::setCallbackValueChange(void (*changeMenuItemValue)(int))
{
  _changeMenuItemValue = changeMenuItemValue;
}

void LCD_Menu::setCallbackValueGet(int (*getMenuItemValue)(int))
{
  _getMenuItemValue = getMenuItemValue;
}

/* Raw code from forum.arduino.cc
 * by Nitro_boy 1-Mar-2011
 * 
 * REQUIRED GLOBAL VARIABLES & DEFINITIONS

#include <LiquidCrystal.h>
#define MOVECURSOR 1  /
#define MOVELIST 2  
byte totalRows = 4;  
byte totalCols = 20;  
unsigned long timeoutTime = 0;  
const int menuTimeout = 10000; 

unsigned long lastButtonPressed; 
const int debounceTime = 50; 
                                         
                                         
LiquidCrystal lcd(10, 11, 12, 13, 14, 15, 16);


   buttonState = digitalRead(buttonPin);
   if (buttonState == HIGH) {     
     AND THEN PUT THE CORRESPONDING CODE
     FROM BELOW HERE
 } 
 
  switch(read_buttons()) 
   {  // analyze encoder response. Default is 0.


   case 1:  

     timeoutTime = millis()+menuTimeout;  // reset timeout timer
     //  if cursor is at top and menu is NOT at top
     //  move menu up one.
     if(cursorPosition == 0 && topItemDisplayed > 0)  //  Cursor is at top of LCD, and there are higher menu items still to be displayed.
     {
       topItemDisplayed--;  // move top menu item displayed up one. 
       redraw = MOVELIST;  // redraw the entire menu
     }

     // if cursor not at top, move it up one.
     if(cursorPosition>0)
     {
       cursorPosition--;  // move cursor up one.
       redraw = MOVECURSOR;  // redraw just cursor.
     }
     break;

   case 2:    

     timeoutTime = millis()+menuTimeout;  // reset timeout timer
     // this sees if there are menu items below the bottom of the LCD screen & sees if cursor is at bottom of LCD 
     if((topItemDisplayed + (totalRows-1)) < totalMenuItems && cursorPosition == (totalRows-1))
     {
       topItemDisplayed++;  // move menu down one
       redraw = MOVELIST;  // redraw entire menu
     }
     if(cursorPosition<(totalRows-1))  // cursor is not at bottom of LCD, so move it down one.
     {
       cursorPosition++;  // move cursor down one
       redraw = MOVECURSOR;  // redraw just cursor.
     }
     break;

   case 4:  

     timeoutTime = millis()+menuTimeout;  // reset timeout timer
     switch(topItemDisplayed + cursorPosition) // adding these values together = where on menuItems cursor is.
     {
     //  put code to be run when specific item is selected in place of the Serial.print filler.
     // the Serial.print code can be removed, but DO NOT change the case & break structure. 
     // (Obviously, you should have as many case instances as you do menu items.)
     case 0:  // menu item 1 selected
       Serial.print("Menu item ");
       Serial.print(topItemDisplayed + cursorPosition);
       Serial.print(" selected - ");
       Serial.println(menuItems[topItemDisplayed + cursorPosition]);
       // there is no menuSubMenu1() function. BUT, to have nested menus,
       // copy this function(i.e. all of basicMenu) into a new function named 
       // whatever you want for your sub menu items and repeat.
//        menuSubMenu1();
       break;

     case 1:  // menu item 2 selected
       Serial.print("Menu item ");
       Serial.print(topItemDisplayed + cursorPosition);
       Serial.print(" selected - ");
       Serial.println(menuItems[topItemDisplayed + cursorPosition]);
       break;

     case 2:  // menu item 3 selected
       Serial.print("Menu item ");
       Serial.print(topItemDisplayed + cursorPosition);
       Serial.print(" selected - ");
       Serial.println(menuItems[topItemDisplayed + cursorPosition]);
       break;

     case 3:  // menu item 4 selected
       Serial.print("Menu item ");
       Serial.print(topItemDisplayed + cursorPosition);
       Serial.print(" selected - ");
       Serial.println(menuItems[topItemDisplayed + cursorPosition]);
       break;

     case 4:  // menu item 5 selected
       Serial.print("Menu item ");
       Serial.print(topItemDisplayed + cursorPosition);
       Serial.print(" selected - ");
       Serial.println(menuItems[topItemDisplayed + cursorPosition]);
       break;

     case 5:  // menu item 6 selected
       Serial.print("Menu item ");
       Serial.print(topItemDisplayed + cursorPosition);
       Serial.print(" selected - ");
       Serial.println(menuItems[topItemDisplayed + cursorPosition]);
       break;


     case 6:  // menu item 7 selected
       Serial.print("Menu item ");
       Serial.print(topItemDisplayed + cursorPosition);
       Serial.print(" selected - ");
       Serial.println(menuItems[topItemDisplayed + cursorPosition]);
       break;
       
       // add as many "case #:" as items you have. You could put 
       //  line separators in menuList and leave out the 
       //  corresponding case, which would mean that nothing
       // would be triggered when user selected the line separator.  
     }
     break;
     
   case 8:  // encoder button was pushed for long time. This corresponds to "Back" or "Cancel" being pushed.
     stillSelecting = false;
     Serial.println("Button held for a long time");
     break;

   }

   switch(redraw){  //  checks if menu should be redrawn at all.
   case MOVECURSOR:  // Only the cursor needs to be moved.
     redraw = false;  // reset flag.
     if (cursorPosition > totalMenuItems) // keeps cursor from moving beyond menu items.
       cursorPosition = totalMenuItems;
     for(i = 0; i < (totalRows); i++){  // loop through all of the lines on the LCD
       lcd.setCursor(0,i);
       lcd.print(" ");                      // and erase the previously displayed cursor
       lcd.setCursor((totalCols-1), i);
       lcd.print(" ");
     }
     lcd.setCursor(0,cursorPosition);      // go to LCD line where new cursor should be & display it.
     lcd.print(">");
     lcd.setCursor((totalCols-1), cursorPosition);
     lcd.print("<");
     break;  // MOVECURSOR break.

   case MOVELIST:  // the entire menu needs to be redrawn
     redraw=MOVECURSOR;  // redraw cursor after clearing LCD and printing menu.
     lcd.clear(); // clear screen so it can be repainted.
     if(totalMenuItems>((totalRows-1))){  // if there are more menu items than LCD rows, then cycle through menu items.
       for (i = 0; i < (totalRows); i++){
         lcd.setCursor(1,i);
         lcd.print(menuItems[topItemDisplayed + i]);
       }
     }
     else{  // if menu has less items than LCD rows, display all available menu items.
       for (i = 0; i < totalMenuItems+1; i++){
         lcd.setCursor(1,i);
         lcd.print(menuItems[topItemDisplayed + i]);
       }
     }
     break;  // MOVELIST break
   }

   if (timeoutTime<millis()){  // user hasn't done anything in awhile
     stillSelecting = false;  // tell loop to bail out.
     /*
     in my main code, I had a function that
      displayed a default screen on the LCD, so
      I would put that function here, and it would
      bail out to the default screen.
      defaultScreen();
      */ /*
   }
 } 


 while (stillSelecting == true);  //
}

void read_buttons(){  // you may need to swap "void" with "int" or "byte"
  int returndata = 0;
  int buttonState; // this might not be the correct declaration.
  // remember to declare what buttonUp, buttonDown, buttonSelect, buttonCancel are
if ((lastButtonPressed + debounceTime) < millis(){  // see if it's time to check the buttons again
  buttonState = digitalRead(buttonUp);
  if (buttonState == HIGH){
    returndata = returndata + 1;
    lastButtonPressed = millis();
}

  buttonState = digitalRead(buttonDown);
  if (buttonState == HIGH){
    returndata = returndata + 2;
    lastButtonPressed = millis();
}

  buttonState = digitalRead(buttonSelect);
  if (buttonState == HIGH){
    returndata = returndata + 4;
    lastButtonPressed = millis();
}

  buttonState = digitalRead(buttonCancel);
  if (buttonState == HIGH){
    returndata = returndata + 8;
    lastButtonPressed = millis();
}
}
 return returndata; // this spits back to the function that calls it the variable returndata.
}

 */
