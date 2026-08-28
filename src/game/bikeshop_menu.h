#ifndef BIKESHOP_MENU_H
#define BIKESHOP_MENU_H

void BikeShopMenu_Show(const char *item1, const char *item2, int price);
int  BikeShopMenu_IsOpen(void);
int  BikeShopMenu_GetResult(void);
void BikeShopMenu_Tick(void);
void BikeShopMenu_PostRender(void);

#endif
