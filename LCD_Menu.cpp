
//#include <SoftwareSerial.h>

#include "LiquidCrystal_PCF8574.h"
#include "LCD_Menu.h"
#include "debug.h"

extern LiquidCrystal_PCF8574 lcd;
extern void debug(debugLevels level, char *sFmt, ...);
extern void lcdPrint(char *sFmt, ...);

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
  debug(L_DEBUG, "menu item count: %d\n", _menu_items_count);
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

void LCD_Menu::begin() {
  // show menu on LCD
  updateLCD();
  //int menuitem = _menu_item_first_visible-1;
  // select first menu item
  _menu_item_selected = 1;
  _updateCursor();
  //lcd.setCursor(_menuitems[menuitem].datacolumn-1, 0);
  //lcd.cursor();
}

void LCD_Menu::end() {
  lcd.noBlink();
  lcd.noCursor();
}

void LCD_Menu::moveUp() {
  debug(L_DEBUG, "moveUp");
  // change selected item
  if (_menu_item_selected > 1) _menu_item_selected--;
  // make the selected item visible if required
  if (_menu_item_selected < _menu_item_first_visible) {
    _moveMenu(false);
  }
  _updateCursor();
  debug(L_DEBUG, " - Selected: %d First: %d\n", _menu_item_selected, _menu_item_first_visible);
}

void LCD_Menu::moveDown() {
  debug(L_DEBUG, "moveDown");
  // move to next menu item
  if (_menu_item_selected < _menu_items_count) _menu_item_selected++;
  // make the selected item visible if required
  if (_menu_item_selected >= (_menu_item_first_visible + _lcd_rows) ) {
    _moveMenu(true);
  }
  _updateCursor();
  debug(L_DEBUG, " - Selected: %d First: %d\n", _menu_item_selected, _menu_item_first_visible);
}

void LCD_Menu::uiSelect(bool longPress) {
  //debug(L_DEBUG, "_uiSelect\n");
  if (!_edit_mode) {
    _setEditMode(true);
  } else {
    _setEditMode(false);
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
  lcdPrint("%u",_getMenuItemValue(_menu_item_selected));
  lcd.setCursor(_menuitems[_menu_item_selected-1].datacolumn, (_menu_item_selected - _menu_item_first_visible));
}

void LCD_Menu::_moveMenu(bool down) {
  debug(L_DEBUG, "First Visible: %d (%d, %d)\n", _menu_item_first_visible, _menu_items_count, _lcd_rows);
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
  // do we need to change edit mode?
  if (enable == getEditMode()) return;

  // edit on
  if (enable) {
    _edit_mode = true;
    //_menu_item_selected = _menu_item_first_visible;
  } else {  // edit off
    _edit_mode = false;
    //_menu_item_selected = 0;
  }
  debug(L_DEBUG, "_edit_mode = %d\n", _edit_mode); 
  _updateCursor();
}

void LCD_Menu::_updateCursor() {
  lcd.blink();
  if (!_edit_mode) {
    lcd.setCursor(_menuitems[_menu_item_selected-1].datacolumn-1, (_menu_item_selected - _menu_item_first_visible) );  
    lcd.cursor();
  }
  if (_edit_mode) {
    lcd.setCursor(_menuitems[_menu_item_selected-1].datacolumn, (_menu_item_selected - _menu_item_first_visible) );
    lcd.noCursor();
  } 
  //  lcd.noBlink();
  //  lcd.noCursor();
  
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


