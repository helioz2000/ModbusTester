/*
 * LCD_Menu.h
 */

#ifndef LCD_Menu_h
#define LCD_Menu_h

typedef struct {
  char text[20];
  uint8_t datacolumn;
  int datamin;
  unsigned int datamax;
} menu_item;

class LCD_Menu {
  public:
    LCD_Menu(uint8_t rows, uint8_t columns);
    void setMenuItems(menu_item* menuitems, uint8_t number_of_items);
    void updateLCD();
    void moveUp();
    void moveDown();
    void uiSelect(bool longPress);
    bool getEditMode();
    int  getSelectedMenuItem();
    void updateSelectedMenuItemValue();
    void begin();
    void end();

    void setCallbackValueChange(void (*changeMenuItemValue)(int));
    void setCallbackValueGet(int (*getMenuItemValue)(int));
    
  private:
    void (*_changeMenuItemValue)(int);
    int (*_getMenuItemValue)(int);
    void _moveMenu(bool down);
    void _setEditMode(bool enable);
    void _updateCursor();
    
    bool _edit_mode;
    uint8_t _lcd_rows;
    uint8_t _lcd_columns;
    uint8_t _menu_item_first_visible;
    uint8_t _menu_item_selected;
    uint8_t _menu_items_count;
    menu_item *_menuitems;
};

#endif
