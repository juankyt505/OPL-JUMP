#include <stdio.h>
#include <strings.h>
#include <dirent.h>
#include <elf-loader.h>
/*
  Copyright 2009, Ifcaro & volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include "include/opl.h"
#include "include/menusys.h"
#include "include/iosupport.h"
#include "include/appsupport.h"
#include "include/renderman.h"
#include "include/fntsys.h"
#include "include/lang.h"
#include "include/themes.h"
#include "include/textures.h"
#include "include/pad.h"
#include "include/gui.h"
#include "include/guigame.h"
#include "include/system.h"
#include "include/ioman.h"
#include "include/ethsupport.h"
#include "include/sound.h"
#include <assert.h>

enum MENU_IDs {
    MENU_SETTINGS = 0,
    MENU_GFX_SETTINGS,
    MENU_AUDIO_SETTINGS,
    MENU_CONTROLLER_SETTINGS,
    MENU_OSD_LANGUAGE_SETTINGS,
    MENU_PARENTAL_LOCK,
    MENU_NET_CONFIG,
    MENU_NET_UPDATE,
    MENU_START_NBD,
    MENU_ABOUT,
    MENU_SAVE_CHANGES,
    MENU_EXIT,
    MENU_POWER_OFF
};

enum GAME_MENU_IDs {
    GAME_COMPAT_SETTINGS = 0,
    GAME_CHEAT_SETTINGS,
    GAME_GSM_SETTINGS,
    GAME_VMC_SETTINGS,
#ifdef PADEMU
    GAME_PADEMU_SETTINGS,
    GAME_PADMACRO_SETTINGS,
#endif
    GAME_OSD_LANGUAGE_SETTINGS,
    GAME_SAVE_CHANGES,
    GAME_TEST_CHANGES,
    GAME_REMOVE_CHANGES,
    GAME_RENAME_GAME,
    GAME_DELETE_GAME,
};

// global menu variables
static menu_list_t *menu;
static menu_list_t *selected_item;

static int actionStatus;
static int itemConfigId;
static config_set_t *itemConfig;

static u8 parentalLockCheckEnabled = 1;

// "main menu submenu"
static submenu_list_t *mainMenu;
// active item in the main menu
static submenu_list_t *mainMenuCurrent;

// "game settings submenu"
static submenu_list_t *gameMenu;
// active item in game settings
static submenu_list_t *gameMenuCurrent;

static submenu_list_t *appMenu;
static submenu_list_t *appMenuCurrent;

static s32 menuSemaId;
static s32 menuListSemaId = -1;
static ee_sema_t menuSema;

static void menuRenameGame(submenu_list_t **submenu)
{
    if (!selected_item->item->current) {
        return;
    }

    if (!gEnableWrite)
        return;

    item_list_t *support = selected_item->item->userdata;

    if (support) {
        if (support->itemRename) {
            if (menuCheckParentalLock() == 0) {
                sfxPlay(SFX_MESSAGE);
                int nameLength = support->itemGetNameLength(support, selected_item->item->current->item.id);
                char newName[nameLength];
                strncpy(newName, selected_item->item->current->item.text, nameLength);
                if (guiShowKeyboard(newName, nameLength)) {
                    guiSwitchScreen(GUI_SCREEN_MAIN);
                    submenuDestroy(submenu);

                    // Only rename the file if the name changed; trying to rename a file with a file name that hasn't changed can cause the file
                    // to be deleted on certain file systems.
                    if (strcmp(newName, selected_item->item->current->item.text) != 0) {
                        support->itemRename(support, selected_item->item->current->item.id, newName);
                        ioPutRequest(IO_MENU_UPDATE_DEFFERED, &support->mode);
                    }
                }
            }
        }
    } else
        guiMsgBox("NULL Support object. Please report", 0, NULL);
}

static void menuDeleteGame(submenu_list_t **submenu)
{
    if (!selected_item->item->current)
        return;

    if (!gEnableWrite)
        return;

    item_list_t *support = selected_item->item->userdata;

    if (support) {
        if (support->itemDelete) {
            if (menuCheckParentalLock() == 0) {
                if (guiMsgBox(_l(_STR_DELETE_WARNING), 1, NULL)) {
                    guiSwitchScreen(GUI_SCREEN_MAIN);
                    submenuDestroy(submenu);
                    support->itemDelete(support, selected_item->item->current->item.id);
                    ioPutRequest(IO_MENU_UPDATE_DEFFERED, &support->mode);
                }
            }
        }
    } else
        guiMsgBox("NULL Support object. Please report", 0, NULL);
}

static void _menuLoadConfig()
{
    WaitSema(menuSemaId);
    if (!itemConfig) {
        item_list_t *list = selected_item->item->userdata;
        itemConfig = list->itemGetConfig(list, itemConfigId);
    }
    actionStatus = 0;
    SignalSema(menuSemaId);
}

static void _menuSaveConfig()
{
    int result;

    WaitSema(menuSemaId);
    result = configWrite(itemConfig);
    itemConfigId = -1; // to invalidate cache and force reload
    actionStatus = 0;
    SignalSema(menuSemaId);

    if (!result)
        setErrorMessage(_STR_ERROR_SAVING_SETTINGS);
}

static void _menuRequestConfig()
{
    WaitSema(menuSemaId);
    if (selected_item->item->current != NULL && itemConfigId != selected_item->item->current->item.id) {
        if (itemConfig) {
            configFree(itemConfig);
            itemConfig = NULL;
        }
        item_list_t *list = selected_item->item->userdata;
        if (itemConfigId == -1 || guiInactiveFrames >= list->delay) {
            itemConfigId = selected_item->item->current->item.id;
            ioPutRequest(IO_CUSTOM_SIMPLEACTION, &_menuLoadConfig);
        }
    } else if (itemConfig)
        actionStatus = 0;

    SignalSema(menuSemaId);
}

config_set_t *menuLoadConfig()
{
    actionStatus = 1;
    itemConfigId = -1;
    guiHandleDeferedIO(&actionStatus, _l(_STR_LOADING_SETTINGS), IO_CUSTOM_SIMPLEACTION, &_menuRequestConfig);
    return itemConfig;
}

// we don't want a pop up when transitioning to or refreshing Game Menu gui.
config_set_t *gameMenuLoadConfig(struct UIItem *ui)
{
    actionStatus = 1;
    itemConfigId = -1;
    guiGameHandleDeferedIO(&actionStatus, ui, IO_CUSTOM_SIMPLEACTION, &_menuRequestConfig);
    return itemConfig;
}

void menuSaveConfig()
{
    actionStatus = 1;
    guiHandleDeferedIO(&actionStatus, _l(_STR_SAVING_SETTINGS), IO_CUSTOM_SIMPLEACTION, &_menuSaveConfig);
}

static void menuInitMainMenu(void)
{
    if (mainMenu)
        submenuDestroy(&mainMenu);

    // initialize the menu
    submenuAppendItem(&mainMenu, -1, NULL, MENU_SETTINGS, _STR_SETTINGS);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_GFX_SETTINGS, _STR_GFX_SETTINGS);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_AUDIO_SETTINGS, _STR_AUDIO_SETTINGS);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_CONTROLLER_SETTINGS, _STR_CONTROLLER_SETTINGS);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_OSD_LANGUAGE_SETTINGS, _STR_OSD_SETTINGS);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_PARENTAL_LOCK, _STR_PARENLOCKCONFIG);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_NET_CONFIG, _STR_NETCONFIG);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_NET_UPDATE, _STR_NET_UPDATE);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_START_NBD, _STR_STARTNBD);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_ABOUT, _STR_ABOUT);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_SAVE_CHANGES, _STR_SAVE_CHANGES);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_EXIT, _STR_EXIT);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_POWER_OFF, _STR_POWEROFF);

    mainMenuCurrent = mainMenu;
}

void menuReinitMainMenu(void)
{
    menuInitMainMenu();
}

void menuInitGameMenu(void)
{
    if (gameMenu)
        submenuDestroy(&gameMenu);

    // initialize the menu
    submenuAppendItem(&gameMenu, -1, NULL, GAME_COMPAT_SETTINGS, _STR_COMPAT_SETTINGS);
    submenuAppendItem(&gameMenu, -1, NULL, GAME_CHEAT_SETTINGS, _STR_CHEAT_SETTINGS);
    submenuAppendItem(&gameMenu, -1, NULL, GAME_GSM_SETTINGS, _STR_GSCONFIG);
    submenuAppendItem(&gameMenu, -1, NULL, GAME_VMC_SETTINGS, _STR_VMC_SCREEN);
#ifdef PADEMU
    submenuAppendItem(&gameMenu, -1, NULL, GAME_PADEMU_SETTINGS, _STR_PADEMUCONFIG);
    submenuAppendItem(&gameMenu, -1, NULL, GAME_PADMACRO_SETTINGS, _STR_PADMACROCONFIG);
#endif
    submenuAppendItem(&gameMenu, -1, NULL, GAME_OSD_LANGUAGE_SETTINGS, _STR_OSD_SETTINGS);
    submenuAppendItem(&gameMenu, -1, NULL, GAME_SAVE_CHANGES, _STR_SAVE_CHANGES);
    submenuAppendItem(&gameMenu, -1, NULL, GAME_TEST_CHANGES, _STR_TEST);
    submenuAppendItem(&gameMenu, -1, NULL, GAME_REMOVE_CHANGES, _STR_REMOVE_ALL_SETTINGS);
    if (gEnableWrite) {
        submenuAppendItem(&gameMenu, -1, NULL, GAME_RENAME_GAME, _STR_RENAME);
        submenuAppendItem(&gameMenu, -1, NULL, GAME_DELETE_GAME, _STR_DELETE);
    }

    gameMenuCurrent = gameMenu;
}

void menuInitAppMenu(void)
{
    if (appMenu)
        submenuDestroy(&appMenu);

    // initialize the menu
    submenuAppendItem(&appMenu, -1, NULL, 0, _STR_RENAME);
    submenuAppendItem(&appMenu, -1, NULL, 1, _STR_DELETE);

    appMenuCurrent = appMenu;
}

// -------------------------------------------------------------------------------------------
// ---------------------------------------- Menu manipulation --------------------------------
// -------------------------------------------------------------------------------------------
void menuInit()
{
    menu = NULL;
    selected_item = NULL;
    itemConfigId = -1;
    itemConfig = NULL;
    mainMenu = NULL;
    mainMenuCurrent = NULL;
    gameMenu = NULL;
    gameMenuCurrent = NULL;
    appMenu = NULL;
    appMenuCurrent = NULL;
    menuInitMainMenu();

    menuSema.init_count = 1;
    menuSema.max_count = 1;
    menuSema.option = 0;
    menuSemaId = CreateSema(&menuSema);
    if (menuListSemaId < 0) {
        menuListSemaId = sbCreateSemaphore();
    }
}

void menuEnd()
{
    // destroy menu
    menu_list_t *cur = menu;

    while (cur) {
        menu_list_t *td = cur;
        cur = cur->next;

        if (td->item)
            submenuDestroy(&(td->item->submenu));

        menuRemoveHints(td->item);

        free(td);
    }

    submenuDestroy(&mainMenu);
    submenuDestroy(&gameMenu);
    submenuDestroy(&appMenu);

    if (itemConfig) {
        configFree(itemConfig);
        itemConfig = NULL;
    }

    DeleteSema(menuSemaId);
    DeleteSema(menuListSemaId);
    menuListSemaId = -1;
}

static menu_list_t *AllocMenuItem(menu_item_t *item)
{
    menu_list_t *it;

    it = malloc(sizeof(menu_list_t));

    it->prev = NULL;
    it->next = NULL;
    it->item = item;

    return it;
}

void menuAppendItem(menu_item_t *item)
{
    assert(item);

    WaitSema(menuListSemaId);

    if (menu == NULL) {
        menu = AllocMenuItem(item);
        selected_item = menu;
    } else {
        menu_list_t *cur = menu;

        // traverse till the end
        while (cur->next)
            cur = cur->next;

        // create new item
        menu_list_t *newitem = AllocMenuItem(item);

        // link
        cur->next = newitem;
        newitem->prev = cur;
    }

    SignalSema(menuListSemaId);
}

static void refreshMenuPosition(void)
{
    // Find the first menu in the list that is visible and set it as the active menu.
    if (menu == NULL)
        return;

    menu_list_t *cur = menu;
    while (cur->item->visible == 0 && cur->next)
        cur = cur->next;

    if (cur->item->visible == 0) {
        // No visible menu was found, just set the current menu to the first one in the list.
        selected_item = menu;
    } else
        selected_item = cur;
}

void submenuRebuildCache(submenu_list_t *submenu)
{
    while (submenu) {
        if (submenu->item.cache_id)
            free(submenu->item.cache_id);
        if (submenu->item.cache_uid)
            free(submenu->item.cache_uid);

        int size = gTheme->gameCacheCount * sizeof(int);
        submenu->item.cache_id = malloc(size);
        memset(submenu->item.cache_id, -1, size);
        submenu->item.cache_uid = malloc(size);
        memset(submenu->item.cache_uid, -1, size);

        submenu = submenu->next;
    }
}

static submenu_list_t *submenuAllocItem(int icon_id, char *text, int id, int text_id)
{
    submenu_list_t *it = (submenu_list_t *)malloc(sizeof(submenu_list_t));

    it->prev = NULL;
    it->next = NULL;
    it->item.icon_id = icon_id;
    it->item.text = text;
    it->item.text_id = text_id;
    it->item.id = id;
    it->item.cache_id = NULL;
    it->item.cache_uid = NULL;
    submenuRebuildCache(it);

    return it;
}

submenu_list_t *submenuAppendItem(submenu_list_t **submenu, int icon_id, char *text, int id, int text_id)
{
    if (*submenu == NULL) {
        *submenu = submenuAllocItem(icon_id, text, id, text_id);
        return *submenu;
    }

    submenu_list_t *cur = *submenu;

    // traverse till the end
    while (cur->next)
        cur = cur->next;

    // create new item
    submenu_list_t *newitem = submenuAllocItem(icon_id, text, id, text_id);

    // link
    cur->next = newitem;
    newitem->prev = cur;

    return newitem;
}

static void submenuDestroyItem(submenu_list_t *submenu)
{
    free(submenu->item.cache_id);
    free(submenu->item.cache_uid);

    free(submenu);
}

void submenuRemoveItem(submenu_list_t **submenu, int id)
{
    submenu_list_t *cur = *submenu;
    submenu_list_t *prev = NULL;

    while (cur) {
        if (cur->item.id == id) {
            submenu_list_t *next = cur->next;

            if (prev)
                prev->next = cur->next;

            if (*submenu == cur)
                *submenu = next;

            submenuDestroyItem(cur);

            cur = next;
        } else {
            prev = cur;
            cur = cur->next;
        }
    }
}

void submenuDestroy(submenu_list_t **submenu)
{
    // destroy sub menu
    submenu_list_t *cur = *submenu;

    while (cur) {
        submenu_list_t *td = cur;
        cur = cur->next;

        submenuDestroyItem(td);
    }

    *submenu = NULL;
}

void menuAddHint(menu_item_t *menu, int text_id, int icon_id)
{
    // allocate a new hint item
    menu_hint_item_t *hint = malloc(sizeof(menu_hint_item_t));

    hint->text_id = text_id;
    hint->icon_id = icon_id;
    hint->next = NULL;

    if (menu->hints) {
        menu_hint_item_t *top = menu->hints;

        // rewind to end
        for (; top->next; top = top->next)
            ;

        top->next = hint;
    } else {
        menu->hints = hint;
    }
}

void menuRemoveHints(menu_item_t *menu)
{
    while (menu->hints) {
        menu_hint_item_t *hint = menu->hints;
        menu->hints = hint->next;
        free(hint);
    }
}

char *menuItemGetText(menu_item_t *it)
{
    if (it->text_id >= 0)
        return _l(it->text_id);
    else
        return it->text;
}

char *submenuItemGetText(submenu_item_t *it)
{
    if (it->text_id >= 0)
        return _l(it->text_id);
    else
        return it->text;
}

static void swap(submenu_list_t *a, submenu_list_t *b)
{
    submenu_list_t *pa, *nb;
    pa = a->prev;
    nb = b->next;

    a->next = nb;
    b->prev = pa;
    b->next = a;
    a->prev = b;

    if (pa)
        pa->next = b;

    if (nb)
        nb->prev = a;
}

// Sorts the given submenu by comparing the on-screen titles
void submenuSort(submenu_list_t **submenu)
{
    // a simple bubblesort
    // *submenu = mergeSort(*submenu);
    submenu_list_t *head;
    int sorted = 0;

    if ((submenu == NULL) || (*submenu == NULL) || ((*submenu)->next == NULL))
        return;

    head = *submenu;

    while (!sorted) {
        sorted = 1;

        submenu_list_t *tip = head;

        while (tip->next) {
            submenu_list_t *nxt = tip->next;

            char *txt1 = submenuItemGetText(&tip->item);
            char *txt2 = submenuItemGetText(&nxt->item);

            int cmp = strcasecmp(txt1, txt2);

            if (cmp > 0) {
                swap(tip, nxt);

                if (tip == head)
                    head = nxt;

                sorted = 0;
            } else {
                tip = tip->next;
            }
        }
    }

    *submenu = head;
}

static void menuNextH()
{
    struct menu_list *next = selected_item->next;
    while (next != NULL && next->item->visible == 0)
        next = next->next;

    // If we found a valid menu transition to it.
    if (next != NULL) {
        selected_item = next;
        itemConfigId = -1;
        sfxPlay(SFX_CURSOR);
    }
}

static void menuPrevH()
{
    struct menu_list *prev = selected_item->prev;
    while (prev != NULL && prev->item->visible == 0)
        prev = prev->prev;

    if (prev != NULL) {
        selected_item = prev;
        itemConfigId = -1;
        sfxPlay(SFX_CURSOR);
    }
}

static void menuFirstPage()
{
    submenu_list_t *cur = selected_item->item->current;
    if (cur) {
        if (cur->prev) {
            sfxPlay(SFX_CURSOR);
        }

        selected_item->item->current = selected_item->item->submenu;
        selected_item->item->pagestart = selected_item->item->current;
    }
}

static void menuLastPage()
{
    submenu_list_t *cur = selected_item->item->current;
    if (cur) {
        if (cur->next) {
            sfxPlay(SFX_CURSOR);
        }
        while (cur->next)
            cur = cur->next; // go to end

        selected_item->item->current = cur;

        int itms = ((items_list_t *)gTheme->itemsList->extended)->displayedItems;
        while (--itms && cur->prev) // and move back to have a full page
            cur = cur->prev;

        selected_item->item->pagestart = cur;
    }
}

static void menuNextV()
{
    submenu_list_t *cur = selected_item->item->current;

    if (cur && cur->next) {
        selected_item->item->current = cur->next;
        sfxPlay(SFX_CURSOR);

        // if the current item is beyond the page start, move the page start one page down
        cur = selected_item->item->pagestart;
        int itms = ((items_list_t *)gTheme->itemsList->extended)->displayedItems + 1;
        while (--itms && cur)
            if (selected_item->item->current == cur)
                return;
            else
                cur = cur->next;

        selected_item->item->pagestart = selected_item->item->current;
    } else { // wrap to start
        menuFirstPage();
    }
}

static void menuPrevV()
{
    submenu_list_t *cur = selected_item->item->current;

    if (cur && cur->prev) {
        selected_item->item->current = cur->prev;
        sfxPlay(SFX_CURSOR);

        // if the current item is on the page start, move the page start one page up
        if (selected_item->item->pagestart == cur) {
            int itms = ((items_list_t *)gTheme->itemsList->extended)->displayedItems + 1; // +1 because the selection will move as well
            while (--itms && selected_item->item->pagestart->prev)
                selected_item->item->pagestart = selected_item->item->pagestart->prev;
        }
    } else { // wrap to end
        menuLastPage();
    }
}

static void menuNextPage()
{
    submenu_list_t *cur = selected_item->item->pagestart;

    if (cur && cur->next) {
        int itms = ((items_list_t *)gTheme->itemsList->extended)->displayedItems + 1;
        sfxPlay(SFX_CURSOR);

        while (--itms && cur->next)
            cur = cur->next;

        selected_item->item->current = cur;
        selected_item->item->pagestart = selected_item->item->current;
    } else { // wrap to start
        menuFirstPage();
    }
}

static void menuPrevPage()
{
    submenu_list_t *cur = selected_item->item->pagestart;

    if (cur && cur->prev) {
        int itms = ((items_list_t *)gTheme->itemsList->extended)->displayedItems + 1;
        sfxPlay(SFX_CURSOR);

        while (--itms && cur->prev)
            cur = cur->prev;

        selected_item->item->current = cur;
        selected_item->item->pagestart = selected_item->item->current;
    } else { // wrap to end
        menuLastPage();
    }
}

void menuSetSelectedItem(menu_item_t *item)
{
    menu_list_t *itm = menu;

    while (itm) {
        if (itm->item == item) {
            selected_item = itm;
            return;
        }

        itm = itm->next;
    }
}

void menuRenderMenu()
{
    guiDrawBGPlasma();

    if (!mainMenu)
        return;

    // draw the animated menu
    if (!mainMenuCurrent)
        mainMenuCurrent = mainMenu;

    submenu_list_t *it = mainMenu;

    // calculate the number of items
    int count = 0;
    int sitem = 0;
    for (; it; count++, it = it->next) {
        if (it == mainMenuCurrent)
            sitem = count;
    }

    int spacing = 25;
    int y = (gTheme->usedHeight >> 1) - (spacing * (count >> 1));
    int cp = 0; // current position
    for (it = mainMenu; it; it = it->next, cp++) {
        // render, advance
        fntRenderString(gTheme->fonts[0], 320, y, ALIGN_CENTER, 0, 0, submenuItemGetText(&it->item), (cp == sitem) ? gTheme->selTextColor : gTheme->textColor);
        y += spacing;
        if (cp == (MENU_ABOUT - 1))
            y += spacing / 2;
    }

    // hints
    guiDrawSubMenuHints();
}

int menuSetParentalLockCheckState(int enabled)
{
    int wasEnabled;

    wasEnabled = parentalLockCheckEnabled;
    parentalLockCheckEnabled = enabled ? 1 : 0;

    return wasEnabled;
}

// Returns the configured parental lock password, or NULL if the parental lock is not set up.
static const char *menuGetParentalLockPassword(void)
{
    const char *parentalLockPassword;
    config_set_t *configOPL = configGetByType(CONFIG_OPL);

    if (configGetStr(configOPL, CONFIG_OPL_PARENTAL_LOCK_PWD, &parentalLockPassword) && (parentalLockPassword[0] != '\0'))
        return parentalLockPassword;

    return NULL;
}

int menuIsParentalLockPasswordSet(void)
{
    return menuGetParentalLockPassword() != NULL;
}

// Prompts for the parental lock password. Returns 0 if access was granted, EACCES otherwise.
static int menuPromptParentalLockPassword(const char *parentalLockPassword)
{
    char password[CONFIG_KEY_VALUE_LEN];
    int result;

    password[0] = '\0';
    if (diaShowKeyb(password, CONFIG_KEY_VALUE_LEN, 1, _l(_STR_PARENLOCK_ENTER_PASSWORD_TITLE))) {
        if (strncmp(parentalLockPassword, password, CONFIG_KEY_VALUE_LEN) == 0) {
            result = 0;
        } else if (strncmp(OPL_PARENTAL_LOCK_MASTER_PASS, password, CONFIG_KEY_VALUE_LEN) == 0) {
            guiMsgBox(_l(_STR_PARENLOCK_DISABLE_WARNING), 0, NULL);

            configRemoveKey(configGetByType(CONFIG_OPL), CONFIG_OPL_PARENTAL_LOCK_PWD);
            saveConfig(CONFIG_OPL, 1);

            result = 0;
        } else {
            guiMsgBox(_l(_STR_PARENLOCK_PASSWORD_INCORRECT), 0, NULL);
            result = EACCES;
        }
    } else // User aborted.
        result = EACCES;

    return result;
}

int menuCheckParentalLock(void)
{
    const char *parentalLockPassword;
    int result;

    result = 0; // Default to unlocked.
    if (parentalLockCheckEnabled) {
        // Prompt for password, only if one was set.
        parentalLockPassword = menuGetParentalLockPassword();
        if (parentalLockPassword != NULL) {
            result = menuPromptParentalLockPassword(parentalLockPassword);
            if (result == 0)
                parentalLockCheckEnabled = 0; // Stop asking for the password.
        }
    }

    return result;
}

int menuCheckGameParentalLock(config_set_t *configSet)
{
    const char *parentalLockPassword;
    int locked = 0;

    if (configSet != NULL)
        configGetInt(configSet, CONFIG_ITEM_PARENTAL_LOCK, &locked);
    if (!locked)
        return 0;

    parentalLockPassword = menuGetParentalLockPassword();
    if (parentalLockPassword == NULL)
        return 0; // No password configured, so there is nothing to enforce.

    // A locked game always asks for the password, even if the settings menus were unlocked earlier in this session.
    return menuPromptParentalLockPassword(parentalLockPassword);
}

static int ps1test_scanned;
static void ps1testBeginScan(int force);
static void ps1testScanStep(void);

void menuHandleInputMenu()
{
    if (getKeyOn(KEY_L3)) {
        ps1testBeginScan(0);
        guiSwitchScreen(GUI_SCREEN_PS1);
        return;
    }

    if (!mainMenu)
        return;

    if (!mainMenuCurrent)
        mainMenuCurrent = mainMenu;

    if (getKey(KEY_UP)) {
        sfxPlay(SFX_CURSOR);
        if (mainMenuCurrent->prev)
            mainMenuCurrent = mainMenuCurrent->prev;
        else // rewind to the last item
            while (mainMenuCurrent->next)
                mainMenuCurrent = mainMenuCurrent->next;
    }

    if (getKey(KEY_DOWN)) {
        sfxPlay(SFX_CURSOR);
        if (mainMenuCurrent->next)
            mainMenuCurrent = mainMenuCurrent->next;
        else
            mainMenuCurrent = mainMenu;
    }

    if (getKeyOn(gSelectButton)) {
        // execute the item via looking at the id of it
        int id = mainMenuCurrent->item.id;

        sfxPlay(SFX_CONFIRM);

        if (id == MENU_SETTINGS) {
            if (menuCheckParentalLock() == 0)
                guiShowConfig();
        } else if (id == MENU_GFX_SETTINGS) {
            if (menuCheckParentalLock() == 0)
                guiShowUIConfig();
        } else if (id == MENU_AUDIO_SETTINGS) {
            if (menuCheckParentalLock() == 0)
                guiShowAudioConfig();
        } else if (id == MENU_CONTROLLER_SETTINGS) {
            if (menuCheckParentalLock() == 0)
                guiShowControllerConfig();
        } else if (id == MENU_OSD_LANGUAGE_SETTINGS) {
            if (menuCheckParentalLock() == 0)
                guiGameShowOSDLanguageConfig(1);
        } else if (id == MENU_PARENTAL_LOCK) {
            if (menuCheckParentalLock() == 0)
                guiShowParentalLockConfig();
        } else if (id == MENU_NET_CONFIG) {
            if (menuCheckParentalLock() == 0)
                guiShowNetConfig();
        } else if (id == MENU_NET_UPDATE) {
            if (menuCheckParentalLock() == 0)
                guiShowNetCompatUpdate();
        } else if (id == MENU_START_NBD) {
            if (menuCheckParentalLock() == 0)
                handleLwnbdSrv();
        } else if (id == MENU_ABOUT) {
            guiShowAbout();
        } else if (id == MENU_SAVE_CHANGES) {
            if (menuCheckParentalLock() == 0) {
                guiGameSaveOSDLanguageGlobalConfig(configGetByType(CONFIG_GAME));
#ifdef PADEMU
                guiGameSavePadEmuGlobalConfig(configGetByType(CONFIG_GAME));
                guiGameSavePadMacroGlobalConfig(configGetByType(CONFIG_GAME));
#endif
                saveConfig(CONFIG_OPL | CONFIG_NETWORK | CONFIG_GAME, 1);
                menuSetParentalLockCheckState(1); // Re-enable parental lock check.
            }
        } else if (id == MENU_EXIT) {
            if (guiMsgBox(_l(_STR_CONFIRMATION_EXIT), 1, NULL))
                sysExecExit();
        } else if (id == MENU_POWER_OFF) {
            if (guiMsgBox(_l(_STR_CONFIRMATION_POFF), 1, NULL))
                sysPowerOff();
        }

        // so the exit press wont propagate twice
        readPads();
    }

    if (getKeyOn(KEY_START) || getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE)) {
        // Check if there is anything to show the user, at all.
        if (gAPPStartMode || gETHStartMode || gBDMStartMode || gHDDStartMode) {
            guiSwitchScreen(GUI_SCREEN_MAIN);
            refreshMenuPosition();
        }
    }
}

static void menuRenderElements(theme_element_t *elem)
{
    // selected_item can't be NULL here as we only allow to switch to "Main" rendering when there is at least one device activated
    _menuRequestConfig();

    WaitSema(menuSemaId);

    while (elem) {
        if (elem->drawElem)
            elem->drawElem(selected_item, selected_item->item->current, itemConfig, elem);

        elem = elem->next;
    }
    SignalSema(menuSemaId);
}

void menuRenderMain(void)
{
    item_list_t *list = selected_item->item->userdata;

    if (list->mode == APP_MODE) {
        menuRenderElements(gTheme->appsMainElems.first);
        gTheme->itemsList = gTheme->appsItemsList;
    } else {
        menuRenderElements(gTheme->mainElems.first);
        gTheme->itemsList = gTheme->gamesItemsList;
    }
}


#define PS1TEST_MAX_GAMES 128
#define PS1TEST_NAME_LEN 128
#define PS1TEST_PATH_LEN 256

static char ps1test_names[PS1TEST_MAX_GAMES][PS1TEST_NAME_LEN];
static char ps1test_paths[PS1TEST_MAX_GAMES][PS1TEST_PATH_LEN];
static unsigned char ps1test_devices[PS1TEST_MAX_GAMES];
#define PS1TEST_DEVICE_BDM 0
#define PS1TEST_DEVICE_SMB 1
#define PS1TEST_DEVICE_HDD 2

static int ps1testGetDevice(const char *path)
{
    if (strncmp(path, "smb0:", 5) == 0)
        return PS1TEST_DEVICE_SMB;

    if (strncmp(path, "pfs", 3) == 0)
        return PS1TEST_DEVICE_HDD;

    return PS1TEST_DEVICE_BDM;
}

static int ps1test_count = 0;
static int ps1test_selected = 0;
static int ps1test_scanned = 0;
static int ps1test_scan_active = 0;
static int ps1test_scan_stage = 0;
static int ps1test_scan_device = 0;
static char ps1test_smb_prefix[PS1TEST_PATH_LEN] = "";

/* Caratulas e iconos de la pagina experimental de PS1. */
static GSTEXTURE ps1test_cov_texture;
static GSTEXTURE ps1test_cov_default_texture;
static int ps1test_cov_default_attempted = 0;
static int ps1test_cov_default_loaded = 0;
static GSTEXTURE ps1test_case_texture;
static int ps1test_case_attempted = 0;
static int ps1test_case_loaded = 0;
static GSTEXTURE ps1test_ico_texture;
static GSTEXTURE ps1test_bg_texture;
static char ps1test_image_path[PS1TEST_PATH_LEN] = "";

static int ps1testThemeUsesGameBackground(void)
{
    theme_element_t *background;
    mutable_image_t *image;

    if (!gTheme)
        return 0;

    background = gTheme->mainElems.first;
    if (!background || !background->extended)
        return 0;

    image = (mutable_image_t *)background->extended;

    return image->cache &&
           image->cache->suffix &&
           strcmp(image->cache->suffix, "BG") == 0;
}

static void ps1testFreeTexture(GSTEXTURE *texture)
{
    if (texture->Mem != NULL)
        rmUnloadTexture(texture);

    texFree(texture);
}

static void ps1testLoadImagesForSelection(void)
{
    const char *path;
    const char *filename;
    const char *slash1;
    const char *slash2;
    const char *slash;
    char covPath[PS1TEST_PATH_LEN + 64];
    char icoPath[PS1TEST_PATH_LEN + 64];
    char bgPath[PS1TEST_PATH_LEN + 64];
    int n;

    if (ps1test_count <= 0 ||
        ps1test_selected < 0 ||
        ps1test_selected >= ps1test_count)
        return;

    path = ps1test_paths[ps1test_selected];

    if (strcmp(path, ps1test_image_path) == 0)
        return;

    ps1testFreeTexture(&ps1test_cov_texture);
    ps1testFreeTexture(&ps1test_ico_texture);
    ps1testFreeTexture(&ps1test_bg_texture);
    ps1test_image_path[0] = '\0';

    /* Aceptar tanto rutas USB con '/' como SMB con '\\'. */
    slash1 = strrchr(path, '/');
    slash2 = strrchr(path, '\\');
    slash = slash1;

    if (slash == NULL || (slash2 != NULL && slash2 > slash))
        slash = slash2;

    if (slash == NULL)
        return;

    filename = slash + 1;

    if (strncmp(path, "smb0:", 5) == 0) {
        item_list_t *eth = ethGetObject(0);
        char *prefix;

        if (eth == NULL || eth->itemGetPrefix == NULL)
            return;

        prefix = eth->itemGetPrefix(eth);
        if (prefix == NULL || strncmp(prefix, "smb0:", 5) != 0)
            return;

        n = snprintf(covPath, sizeof(covPath),
                     "%s%sART\\%s_COV",
                     prefix,
                     (prefix[strlen(prefix) - 1] == '\\' ||
                      prefix[strlen(prefix) - 1] == '/') ? "" : "\\",
                     filename);
        if (n < 0 || (size_t)n >= sizeof(covPath))
            return;

        n = snprintf(icoPath, sizeof(icoPath),
                     "%s%sART\\%s_ICO",
                     prefix,
                     (prefix[strlen(prefix) - 1] == '\\' ||
                      prefix[strlen(prefix) - 1] == '/') ? "" : "\\",
                     filename);
        if (n < 0 || (size_t)n >= sizeof(icoPath))
            return;

        texDiscoverLoad(&ps1test_cov_texture, covPath, -1);
        texDiscoverLoad(&ps1test_ico_texture, icoPath, -1);

        if (ps1testThemeUsesGameBackground()) {
            n = snprintf(bgPath, sizeof(bgPath),
                         "%s%sART\\%s_BG",
                         prefix,
                         (prefix[strlen(prefix) - 1] == '\\' ||
                          prefix[strlen(prefix) - 1] == '/') ? "" : "\\",
                         filename);
            if (n >= 0 && (size_t)n < sizeof(bgPath))
                texDiscoverLoad(&ps1test_bg_texture, bgPath, -1);
        }
    } else if (strncmp(path, "mass", 4) == 0 &&
               path[4] >= '0' && path[4] <= '4' &&
               path[5] == ':') {
        n = snprintf(covPath, sizeof(covPath),
                     "%.6s/ART/%s_COV", path, filename);
        if (n < 0 || (size_t)n >= sizeof(covPath))
            return;

        n = snprintf(icoPath, sizeof(icoPath),
                     "%.6s/ART/%s_ICO", path, filename);
        if (n < 0 || (size_t)n >= sizeof(icoPath))
            return;

        texDiscoverLoad(&ps1test_cov_texture, covPath, -1);
        texDiscoverLoad(&ps1test_ico_texture, icoPath, -1);

        if (ps1testThemeUsesGameBackground()) {
            n = snprintf(bgPath, sizeof(bgPath),
                         "%.6s/ART/%s_BG", path, filename);
            if (n >= 0 && (size_t)n < sizeof(bgPath))
                texDiscoverLoad(&ps1test_bg_texture, bgPath, -1);
        }
    } else if (strncmp(path, "pfs", 3) == 0) {
        const char *appsPath = strstr(path, "APPS/");
        size_t prefixLen;

        if (appsPath == NULL)
            appsPath = strstr(path, "APPS\\\\");

        if (appsPath == NULL)
            return;

        prefixLen = (size_t)(appsPath - path);

        n = snprintf(covPath, sizeof(covPath),
                     "%.*sART/%s_COV",
                     (int)prefixLen, path, filename);
        if (n < 0 || (size_t)n >= sizeof(covPath))
            return;

        n = snprintf(icoPath, sizeof(icoPath),
                     "%.*sART/%s_ICO",
                     (int)prefixLen, path, filename);
        if (n < 0 || (size_t)n >= sizeof(icoPath))
            return;

        texDiscoverLoad(&ps1test_cov_texture, covPath, -1);
        texDiscoverLoad(&ps1test_ico_texture, icoPath, -1);

        if (ps1testThemeUsesGameBackground()) {
            n = snprintf(bgPath, sizeof(bgPath),
                         "%.*sART/%s_BG",
                         (int)prefixLen, path, filename);
            if (n >= 0 && (size_t)n < sizeof(bgPath))
                texDiscoverLoad(&ps1test_bg_texture, bgPath, -1);
        }
    } else {
        return;
    }

    strncpy(ps1test_image_path, path, sizeof(ps1test_image_path) - 1);
    ps1test_image_path[sizeof(ps1test_image_path) - 1] = '\0';
}

static int ps1testIsElf(const char *name)
{
    if (!name)
        return 0;

    size_t len = strlen(name);
    if (len < 7 || strcasecmp(name + len - 4, ".ELF") != 0)
        return 0;

    return strncasecmp(name, "XX.", 3) == 0 ||
           strncasecmp(name, "SB.", 3) == 0 ||
           strncasecmp(name, "SCUS_", 5) == 0 ||
           strncasecmp(name, "SLUS_", 5) == 0 ||
           strncasecmp(name, "SLES_", 5) == 0 ||
           strncasecmp(name, "SCES_", 5) == 0;
}
static int ps1testJoinPath(char *out, size_t outSize,
                           const char *base, const char *leaf)
{
    size_t len;
    char separator = (strncmp(base, "smb0:", 5) == 0) ? '\\' : '/';
    int n;

    if (!out || outSize == 0 || !base || !leaf)
        return 0;

    len = strlen(base);
    while (len > 0 && (base[len - 1] == '/' || base[len - 1] == '\\'))
        len--;

    n = snprintf(out, outSize, "%.*s%c%s",
                 (int)len, base, separator, leaf);

    return n >= 0 && (size_t)n < outSize;
}

static void ps1testScanDirectory(const char *dir)
{
    DIR *dp = opendir(dir);
    if (!dp)
        return;

    struct dirent *entry;
    while ((entry = readdir(dp)) != NULL) {
        if (!ps1testIsElf(entry->d_name))
            continue;
        if (ps1test_count >= PS1TEST_MAX_GAMES)
            break;

        char fullpath[PS1TEST_PATH_LEN];
        if (!ps1testJoinPath(fullpath, sizeof(fullpath), dir, entry->d_name))
            continue;

        int duplicate = 0;
        for (int i = 0; i < ps1test_count; i++) {
            if (strcmp(ps1test_paths[i], fullpath) == 0) {
                duplicate = 1;
                break;
            }
        }
        if (duplicate)
            continue;

        strncpy(ps1test_paths[ps1test_count], fullpath, PS1TEST_PATH_LEN - 1);
        ps1test_paths[ps1test_count][PS1TEST_PATH_LEN - 1] = '\0';

        /* Ocultar el prefijo XX./SB. y la extensión .ELF en la lista. */
        const char *visible = entry->d_name + 3;
        size_t vlen = strlen(visible);
        if (vlen >= 4) {
            size_t copylen = vlen - 4;
            if (copylen >= PS1TEST_NAME_LEN)
                copylen = PS1TEST_NAME_LEN - 1;
            memcpy(ps1test_names[ps1test_count], visible, copylen);
            ps1test_names[ps1test_count][copylen] = '\0';
        } else {
            strncpy(ps1test_names[ps1test_count], entry->d_name, PS1TEST_NAME_LEN - 1);
            ps1test_names[ps1test_count][PS1TEST_NAME_LEN - 1] = '\0';
        }

        ps1test_devices[ps1test_count] =
            ps1testGetDevice(fullpath);
        ps1test_count++;
    }

    closedir(dp);
}

static void ps1testTrim(char *s)
{
    char *p = s;
    while (*p == ' ' || *p == '\t')
        p++;
    if (p != s)
        memmove(s, p, strlen(p) + 1);

    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' ||
                     s[n - 1] == '\r' || s[n - 1] == '\n'))
        s[--n] = '\0';
}

static void ps1testScanApps(const char *appsDir)
{
    DIR *dp = opendir(appsDir);
    if (!dp)
        return;

    struct dirent *entry;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.' &&
            (entry->d_name[1] == '\0' ||
             (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
            continue;

        char folder[PS1TEST_PATH_LEN];
        char cfgPath[PS1TEST_PATH_LEN];
        if (!ps1testJoinPath(folder, sizeof(folder), appsDir, entry->d_name))
            continue;
        if (!ps1testJoinPath(cfgPath, sizeof(cfgPath), folder, "title.cfg"))
            continue;

        /* Solo procesar subcarpetas que contengan title.cfg. */
        FILE *cfg = fopen(cfgPath, "r");
        if (!cfg)
            continue;

        char title[PS1TEST_NAME_LEN] = "";
        char boot[PS1TEST_NAME_LEN] = "";
        char line[256];

        while (fgets(line, sizeof(line), cfg)) {
            ps1testTrim(line);
            if (strncmp(line, "title=", 6) == 0) {
                strncpy(title, line + 6, sizeof(title) - 1);
                title[sizeof(title) - 1] = '\0';
                ps1testTrim(title);
            } else if (strncmp(line, "boot=", 5) == 0) {
                strncpy(boot, line + 5, sizeof(boot) - 1);
                boot[sizeof(boot) - 1] = '\0';
                ps1testTrim(boot);
            }
        }
        fclose(cfg);

        /* Ignorar aplicaciones normales: solo accesos POPStarter XX./SB. */
        if (!title[0] || !ps1testIsElf(boot))
            continue;

        char fullpath[PS1TEST_PATH_LEN];
        if (!ps1testJoinPath(fullpath, sizeof(fullpath), folder, boot))
            continue;

        FILE *elf = fopen(fullpath, "rb");
        if (!elf)
            continue;
        fclose(elf);

        int duplicate = 0;
        for (int i = 0; i < ps1test_count; i++) {
            if (strcmp(ps1test_paths[i], fullpath) == 0) {
                duplicate = 1;
                break;
            }
        }
        if (duplicate)
            continue;
        if (ps1test_count >= PS1TEST_MAX_GAMES)
            break;

        strncpy(ps1test_names[ps1test_count], title,
                PS1TEST_NAME_LEN - 1);
        ps1test_names[ps1test_count][PS1TEST_NAME_LEN - 1] = '\0';

        strncpy(ps1test_paths[ps1test_count], fullpath,
                PS1TEST_PATH_LEN - 1);
        ps1test_paths[ps1test_count][PS1TEST_PATH_LEN - 1] = '\0';

        ps1test_devices[ps1test_count] =
            ps1testGetDevice(fullpath);
        ps1test_count++;
    }

    closedir(dp);
}

static void ps1testSortByDevice(void)
{
    int i;

    /* Orden estable: BDM, SMB y HDD.
       Se mueven juntos nombre, ruta y dispositivo. */
    for (i = 1; i < ps1test_count; i++) {
        char name[PS1TEST_NAME_LEN];
        char path[PS1TEST_PATH_LEN];
        unsigned char device = ps1test_devices[i];
        int j = i;

        memcpy(name, ps1test_names[i], sizeof(name));
        memcpy(path, ps1test_paths[i], sizeof(path));

        while (j > 0 && ps1test_devices[j - 1] > device) {
            memcpy(ps1test_names[j], ps1test_names[j - 1],
                   sizeof(ps1test_names[j]));
            memcpy(ps1test_paths[j], ps1test_paths[j - 1],
                   sizeof(ps1test_paths[j]));
            ps1test_devices[j] = ps1test_devices[j - 1];
            j--;
        }

        memcpy(ps1test_names[j], name, sizeof(name));
        memcpy(ps1test_paths[j], path, sizeof(path));
        ps1test_devices[j] = device;
    }
}

static void ps1testBeginScan(int force)
{
    if (ps1test_scan_active)
        return;

    if (!force && ps1test_scanned)
        return;

    ps1test_count = 0;
    ps1test_selected = 0;
    ps1test_scanned = 0;
    ps1test_scan_device = 0;
    ps1test_scan_stage = 1;
    ps1test_smb_prefix[0] = '\0';
    ps1test_scan_active = 1;
}

static void ps1testScanStep(void)
{
    char path[PS1TEST_PATH_LEN];
    int n;

    if (!ps1test_scan_active)
        return;

    /* Etapas 1-3: raiz, POPS y APPS de cada dispositivo USB. */
    if (ps1test_scan_stage >= 1 && ps1test_scan_stage <= 3) {
        if (ps1test_scan_stage == 1) {
            snprintf(path, sizeof(path), "mass%d:",
                     ps1test_scan_device);
            ps1testScanDirectory(path);
            ps1test_scan_stage = 2;
            return;
        }

        if (ps1test_scan_stage == 2) {
            snprintf(path, sizeof(path), "mass%d:/POPS",
                     ps1test_scan_device);
            ps1testScanDirectory(path);
            ps1test_scan_stage = 3;
            return;
        }

        snprintf(path, sizeof(path), "mass%d:/APPS",
                 ps1test_scan_device);
        ps1testScanApps(path);

        ps1test_scan_device++;
        if (ps1test_scan_device < 5)
            ps1test_scan_stage = 1;
        else
            ps1test_scan_stage = 10;
        return;
    }

    /* Etapa 10: APPS del HDD interno, si esta disponible. */
    if (ps1test_scan_stage == 10) {
        item_list_t *hdd = hddGetObject(0);

        if (hdd != NULL && hdd->itemGetPrefix != NULL) {
            char *prefix = hdd->itemGetPrefix(hdd);

            if (prefix != NULL && prefix[0] != '\0') {
                int pathOK;

                if (strcmp(prefix, "pfs0:") == 0) {
                    n = snprintf(path, sizeof(path), "%sAPPS", prefix);
                    pathOK = n >= 0 && (size_t)n < sizeof(path);
                } else {
                    pathOK = ps1testJoinPath(
                        path, sizeof(path), prefix, "APPS");
                }

                if (pathOK)
                    ps1testScanApps(path);
            }
        }

        ps1test_scan_stage = 11;
        return;
    }

    /* Etapa 11: detectar SMB y leer su raiz. */
    if (ps1test_scan_stage == 11) {
        ps1test_smb_prefix[0] = '\0';

        if (gNetworkStartup == 0) {
            item_list_t *eth = ethGetObject(0);

            if (eth != NULL && eth->itemGetPrefix != NULL) {
                char *prefix = eth->itemGetPrefix(eth);

                if (prefix != NULL && prefix[0] != '\0') {
                    strncpy(ps1test_smb_prefix, prefix,
                            sizeof(ps1test_smb_prefix) - 1);
                    ps1test_smb_prefix[
                        sizeof(ps1test_smb_prefix) - 1] = '\0';

                    ps1testScanDirectory(ps1test_smb_prefix);
                }
            }
        }

        ps1test_scan_stage = 12;
        return;
    }

    /* Etapa 12: POPS de SMB. */
    if (ps1test_scan_stage == 12) {
        if (ps1test_smb_prefix[0] != '\0' &&
            ps1testJoinPath(path, sizeof(path),
                            ps1test_smb_prefix, "POPS"))
            ps1testScanDirectory(path);

        ps1test_scan_stage = 13;
        return;
    }

    /* Etapa 13: APPS de SMB. */
    if (ps1test_scan_stage == 13) {
        if (ps1test_smb_prefix[0] != '\0' &&
            ps1testJoinPath(path, sizeof(path),
                            ps1test_smb_prefix, "APPS"))
            ps1testScanApps(path);

        ps1test_scan_stage = 14;
        return;
    }

    /* Etapa final: conservar el orden actual y publicar la lista. */
    if (ps1test_scan_stage == 14) {
        /* PRUEBA: ordenamiento desactivado temporalmente. */
        ps1test_scanned = 1;
        ps1test_scan_active = 0;
        ps1test_scan_stage = 0;
    }
}

static void ps1testDrawBackground(void)
{
    theme_element_t *background =
        gTheme ? gTheme->mainElems.first : NULL;

    if (!background) {
        guiDrawBGPlasma();
        return;
    }

    if (ps1testThemeUsesGameBackground() &&
        ps1test_bg_texture.Mem != NULL) {
        rmDrawPixmap(&ps1test_bg_texture,
                     background->posX, background->posY,
                     background->aligned, background->width,
                     background->height, background->scaled,
                     gDefaultCol);
        return;
    }

    /* Reutilizar el fondo fijo o el plasma definidos por el tema. */
    if (background->drawElem)
        background->drawElem(NULL, NULL, NULL, background);
    else
        guiDrawBGPlasma();
}

void menuRenderPS1(void)
{
    if (!ps1test_scan_active && ps1test_count > 0)
        ps1testLoadImagesForSelection();

    guiDrawBGPlasma();

    /* Encabezado JUMP: portal unificado para múltiples dispositivos. */
    fntRenderString(gTheme->fonts[0], 320, 10, ALIGN_CENTER, 0, 0,
                    "JUMP", gTheme->selTextColor);
    fntRenderString(gTheme->fonts[0], 320, 39, ALIGN_CENTER, 0, 0,
                    "UNIFIED MULTI-DEVICE PORTAL",
                    gTheme->textColor);

    if (ps1test_scan_active) {
        fntRenderString(gTheme->fonts[0], 320, 175,
                        ALIGN_CENTER, 0, 0,
                        _l(_STR_PS1TEST_SCANNING),
                        gTheme->selTextColor);

        char debugStage[32];
        snprintf(debugStage, sizeof(debugStage), "STAGE: %d  GAMES: %d",
                 ps1test_scan_stage, ps1test_count);
        fntRenderString(gTheme->fonts[0], 320, 205,
                        ALIGN_CENTER, 0, 0, debugStage,
                        gTheme->selTextColor);

        if (gTheme && gTheme->loadingIcon &&
            gTheme->loadingIconCount > 0) {
            GSTEXTURE *loading = thmGetTexture(
                LOAD0_ICON +
                ((guiFrameId >> 1) % gTheme->loadingIconCount));

            if (loading && loading->Mem) {
                rmDrawPixmap(loading,
                    gTheme->loadingIcon->posX,
                    gTheme->loadingIcon->posY,
                    gTheme->loadingIcon->aligned,
                    gTheme->loadingIcon->width,
                    gTheme->loadingIcon->height,
                    gTheme->loadingIcon->scaled,
                    gDefaultCol);
            }
        }

        fntRenderString(gTheme->fonts[0], 320, 450,
                        ALIGN_CENTER, 0, 0,
                        "START: BACK",
                        gTheme->textColor);
        return;
    }

    if (ps1test_count == 0) {
        fntRenderString(gTheme->fonts[0], 320, 180, ALIGN_CENTER, 0, 0,
                        "No XX.*.ELF or SB.*.ELF files found",
                        gTheme->textColor);
    } else {
        int first = ps1test_selected - 4;
        if (first < 0)
            first = 0;
        int last = first + 8;
        if (last > ps1test_count)
            last = ps1test_count;

        int y = 75;
        int previousDevice = -1;

        for (int i = first; i < last; i++) {
            if (ps1test_devices[i] != previousDevice) {
                const char *header = "BDM";
                if (ps1test_devices[i] == PS1TEST_DEVICE_SMB)
                    header = "SMB";
                else if (ps1test_devices[i] == PS1TEST_DEVICE_HDD)
                    header = "HDD";

                fntRenderString(gTheme->fonts[0], 165, y,
                                ALIGN_CENTER, 0, 0,
                                header, gTheme->selTextColor);
                y += 24;
                previousDevice = ps1test_devices[i];
            }

            fntRenderString(gTheme->fonts[0], 165, y,
                            ALIGN_CENTER, 0, 0,
                            ps1test_names[i],
                            i == ps1test_selected ?
                            gTheme->selTextColor : gTheme->textColor);
            y += 24;
        }

        /* Indicadores de juegos fuera de la zona visible. */
        if (first > 0) {
            rmDrawLine(300, 65, 310, 55, gTheme->selTextColor);
            rmDrawLine(310, 55, 320, 65, gTheme->selTextColor);
        }

        if (last < ps1test_count) {
            rmDrawLine(300, 330, 310, 340, gTheme->selTextColor);
            rmDrawLine(310, 340, 320, 330, gTheme->selTextColor);
            fntRenderString(gTheme->fonts[0], 165, 355,
                            ALIGN_CENTER, 0, 0,
                            _l(_STR_PS1TEST_MORE_SHORTCUTS),
                            gTheme->selTextColor);
        }

        /* Dibujar la caratula personalizada o la predeterminada de OPL. */
        GSTEXTURE *cover = &ps1test_cov_texture;

        if (cover->Mem == NULL) {
            if (!ps1test_cov_default_attempted) {
                ps1test_cov_default_attempted = 1;
                ps1test_cov_default_loaded =
                    (texLoadInternal(&ps1test_cov_default_texture,
                                     COVER_DEFAULT) >= 0);
            }

            if (ps1test_cov_default_loaded)
                cover = &ps1test_cov_default_texture;
        }

        if (cover->Mem != NULL) {
            if (!ps1test_case_attempted) {
                ps1test_case_attempted = 1;
                ps1test_case_loaded =
                    (texLoadInternal(&ps1test_case_texture,
                                     APPS_CASE_OVERLAY) >= 0);
            }

            if (ps1test_case_loaded) {
                rmDrawOverlayPixmap(&ps1test_case_texture,
                                    465, 255, ALIGN_CENTER,
                                    256, 256, SCALING_RATIO, gDefaultCol,
                                    cover,
                                    0, 9, 168, 19,
                                    0, 170, 168, 180);
            } else {
                rmDrawPixmap(cover, 465, 255, ALIGN_CENTER,
                             190, 260, SCALING_RATIO, gDefaultCol);
            }
        }

        if (ps1test_ico_texture.Mem != NULL)
            rmDrawPixmap(&ps1test_ico_texture, 555, 100, ALIGN_CENTER,
                         56, 56, SCALING_RATIO, gDefaultCol);

        char status[64];
        snprintf(status, sizeof(status), _l(_STR_PS1TEST_SHORTCUTS_FOUND), ps1test_count);
        fntRenderString(gTheme->fonts[0], 320, 420, ALIGN_CENTER, 0, 0,
                        status, gTheme->selTextColor);
    }

    fntRenderString(gTheme->fonts[0], 320, 450, ALIGN_CENTER, 0, 0,
                    "X: OPEN  |  TRIANGLE: REFRESH  |  START: BACK",
                    gTheme->textColor);
}

static void ps1testLaunchSelected(void)
{
    char filename[PS1TEST_PATH_LEN];
    char partition[128] = "";
    FILE *check;
    int device;

    if (ps1test_count <= 0 || ps1test_selected < 0 ||
        ps1test_selected >= ps1test_count)
        return;

    strncpy(filename, ps1test_paths[ps1test_selected],
            sizeof(filename) - 1);
    filename[sizeof(filename) - 1] = '\0';

    if (strncmp(filename, "smb0:", 5) == 0) {
        device = ETH_MODE;
    } else if (strncmp(filename, "mass", 4) == 0 &&
               filename[4] >= '0' && filename[4] <= '4' &&
               filename[5] == ':') {
        device = filename[4] - '0';
    } else {
        if (oplPath2Mode(filename) != HDD_MODE) {
            guiMsgBox("Ruta de ELF no valida", 0, NULL);
            return;
        }

        device = HDD_MODE;
        snprintf(partition, sizeof(partition), "%s:", gOPLPart);
    }

    check = fopen(filename, "rb");
    if (check == NULL) {
        guiMsgBox("No se puede abrir el archivo ELF", 0, NULL);
        return;
    }
    fclose(check);

    deinit(UNMOUNT_EXCEPTION, device);
    LoadELFFromFileWithPartition(filename, partition, 0, NULL);
}

void menuHandleInputPS1(void)
{
    if (getKeyOn(KEY_START) || getKeyOn(KEY_CIRCLE)) {
        guiSwitchScreen(GUI_SCREEN_MAIN);
        readPads();
        return;
    }

    if (ps1test_scan_active) {
        ps1testScanStep();
        return;
    }

    if (getKeyOn(KEY_UP) && ps1test_count > 0) {
        if (ps1test_selected > 0)
            ps1test_selected--;
        else
            ps1test_selected = ps1test_count - 1;
        sfxPlay(SFX_CURSOR);
    }

    if (getKeyOn(KEY_DOWN) && ps1test_count > 0) {
        if (ps1test_selected < ps1test_count - 1)
            ps1test_selected++;
        else
            ps1test_selected = 0;
        sfxPlay(SFX_CURSOR);
    }

    if (getKeyOn(KEY_CROSS) && ps1test_count > 0) {
        ps1testLaunchSelected();
        return;
    }

    if (getKeyOn(KEY_TRIANGLE)) {
        /* Liberar las imagenes anteriores antes de actualizar la lista. */
        ps1testFreeTexture(&ps1test_cov_texture);
        ps1testFreeTexture(&ps1test_ico_texture);
        ps1testFreeTexture(&ps1test_bg_texture);
        ps1test_image_path[0] = '\0';

        ps1testBeginScan(1);
        sfxPlay(SFX_CONFIRM);
    }
}


void menuHandleInputMain()
{
    if (getKeyOn(KEY_L3)) {
        item_list_t *support = NULL;

        if (selected_item != NULL && selected_item->item != NULL)
            support = selected_item->item->userdata;

        if (support != NULL && support->mode == APP_MODE) {
            appTogglePS1Filter();
            ioPutRequest(IO_MENU_UPDATE_DEFFERED, &support->mode);
            sfxPlay(SFX_CONFIRM);
            return;
        }

        ps1testBeginScan(0);
        guiSwitchScreen(GUI_SCREEN_PS1);
        return;
    }

    if (getKey(KEY_LEFT)) {
        menuPrevH();
    } else if (getKey(KEY_RIGHT)) {
        menuNextH();
    } else if (getKey(KEY_UP)) {
        menuPrevV();
    } else if (getKey(KEY_DOWN)) {
        menuNextV();
    } else if (getKeyOn(KEY_CROSS)) {
        selected_item->item->execCross(selected_item->item);
    } else if (getKeyOn(KEY_TRIANGLE)) {
        selected_item->item->execTriangle(selected_item->item);
    } else if (getKeyOn(KEY_CIRCLE)) {
        selected_item->item->execCircle(selected_item->item);
    } else if (getKeyOn(KEY_SQUARE)) {
        selected_item->item->execSquare(selected_item->item);
    } else if (getKeyOn(KEY_START)) {
        // reinit main menu - show/hide items valid in the active context
        menuInitMainMenu();
        guiSwitchScreen(GUI_SCREEN_MENU);
    } else if (getKeyOn(KEY_SELECT)) {
        selected_item->item->refresh(selected_item->item);
    } else if (getKey(KEY_L1)) {
        menuPrevPage();
    } else if (getKey(KEY_R1)) {
        menuNextPage();
    } else if (getKeyOn(KEY_L2)) { // home
        menuFirstPage();
    } else if (getKeyOn(KEY_R2)) { // end
        menuLastPage();
    }

    // Last Played Auto Start
    if (RemainSecs < 0) {
        DisableCron = 1; // Disable Counter
        RemainSecs = 0;  // Reset, so a launch that returns (e.g. a cancelled parental lock prompt) does not retrigger every frame
        if (gSelectButton == KEY_CIRCLE)
            selected_item->item->execCircle(selected_item->item);
        else
            selected_item->item->execCross(selected_item->item);
    }
}

void menuRenderInfo(void)
{
    item_list_t *list = selected_item->item->userdata;

    if (list->mode == APP_MODE) {
        menuRenderElements(gTheme->appsInfoElems.first);
        gTheme->itemsList = gTheme->appsItemsList;
    } else {
        menuRenderElements(gTheme->infoElems.first);
        gTheme->itemsList = gTheme->gamesItemsList;
    }
}

void menuHandleInputInfo()
{
    if (getKeyOn(KEY_CROSS)) {
        if (gSelectButton == KEY_CIRCLE)
            guiSwitchScreen(GUI_SCREEN_MAIN);
        else
            selected_item->item->execCross(selected_item->item);
    } else if (getKey(KEY_UP)) {
        menuPrevV();
    } else if (getKey(KEY_DOWN)) {
        menuNextV();
    } else if (getKeyOn(KEY_CIRCLE)) {
        if (gSelectButton == KEY_CROSS)
            guiSwitchScreen(GUI_SCREEN_MAIN);
        else
            selected_item->item->execCircle(selected_item->item);
    } else if (getKey(KEY_L1)) {
        menuPrevPage();
    } else if (getKey(KEY_R1)) {
        menuNextPage();
    } else if (getKeyOn(KEY_L2)) {
        menuFirstPage();
    } else if (getKeyOn(KEY_R2)) {
        menuLastPage();
    }
}

void menuRenderGameMenu()
{
    guiDrawBGPlasma();

    if (!gameMenu)
        return;

    // If the device menu that has the selected game suddenly goes invisible (device was removed), switch
    // back to the game list menu.
    if (selected_item->item->visible == 0) {
        guiSwitchScreen(GUI_SCREEN_MAIN);
        return;
    }

    // If we enter the game settings menu and there's no selected item bail out. I'm not entirely sure how we get into
    // this state but it seems to happen on some consoles when transitioning from the game settings menu back to the game
    // list menu.
    if (selected_item->item->current == NULL)
        return;

    // draw the animated menu
    if (!gameMenuCurrent)
        gameMenuCurrent = gameMenu;

    submenu_list_t *it = gameMenu;

    // calculate the number of items
    int count = 0;
    int sitem = 0;
    for (; it; count++, it = it->next) {
        if (it == gameMenuCurrent)
            sitem = count;
    }

    int spacing = 25;
    int y = (gTheme->usedHeight >> 1) - (spacing * (count >> 1));
    int cp = 0; // current position

    // game title
    fntRenderString(gTheme->fonts[0], 320, 20, ALIGN_CENTER, 0, 0, selected_item->item->current->item.text, gTheme->selTextColor);

    // config source
    char *cfgSource = gameConfigSource();
    fntRenderString(gTheme->fonts[0], 320, 40, ALIGN_CENTER, 0, 0, cfgSource, gTheme->textColor);

    // settings list
    for (it = gameMenu; it; it = it->next, cp++) {
        // render, advance
        fntRenderString(gTheme->fonts[0], 320, y, ALIGN_CENTER, 0, 0, submenuItemGetText(&it->item), (cp == sitem) ? gTheme->selTextColor : gTheme->textColor);
        y += spacing;
        if (cp == (GAME_SAVE_CHANGES - 1) || cp == (GAME_REMOVE_CHANGES - 1))
            y += spacing / 2;
    }

    // hints
    guiDrawSubMenuHints();
}

void menuHandleInputGameMenu()
{
    if (!gameMenu)
        return;

    if (!gameMenuCurrent)
        gameMenuCurrent = gameMenu;

    if (getKey(KEY_UP)) {
        sfxPlay(SFX_CURSOR);
        if (gameMenuCurrent->prev)
            gameMenuCurrent = gameMenuCurrent->prev;
        else // rewind to the last item
            while (gameMenuCurrent->next)
                gameMenuCurrent = gameMenuCurrent->next;
    }

    if (getKey(KEY_DOWN)) {
        sfxPlay(SFX_CURSOR);
        if (gameMenuCurrent->next)
            gameMenuCurrent = gameMenuCurrent->next;
        else
            gameMenuCurrent = gameMenu;
    }

    if (getKeyOn(gSelectButton)) {
        // execute the item via looking at the id of it
        int menuID = gameMenuCurrent->item.id;

        sfxPlay(SFX_CONFIRM);

        if (menuID == GAME_COMPAT_SETTINGS) {
            guiGameShowCompatConfig(selected_item->item->current->item.id, selected_item->item->userdata, itemConfig);
        } else if (menuID == GAME_CHEAT_SETTINGS) {
            guiGameShowCheatConfig();
        } else if (menuID == GAME_GSM_SETTINGS) {
            guiGameShowGSConfig();
        } else if (menuID == GAME_VMC_SETTINGS) {
            guiGameShowVMCMenu(selected_item->item->current->item.id, selected_item->item->userdata);
#ifdef PADEMU
        } else if (menuID == GAME_PADEMU_SETTINGS) {
            guiGameShowPadEmuConfig(0);
        } else if (menuID == GAME_PADMACRO_SETTINGS) {
            guiGameShowPadMacroConfig(0);
#endif
        } else if (menuID == GAME_OSD_LANGUAGE_SETTINGS) {
            guiGameShowOSDLanguageConfig(0);
        } else if (menuID == GAME_SAVE_CHANGES) {
            if (guiGameSaveConfig(itemConfig, selected_item->item->userdata))
                configSetInt(itemConfig, CONFIG_ITEM_CONFIGSOURCE, CONFIG_SOURCE_USER);
            menuSaveConfig();
            saveConfig(CONFIG_GAME, 0);
            menuSetParentalLockCheckState(1); // Re-enable parental lock check.
            guiMsgBox(_l(_STR_GAME_SETTINGS_SAVED), 0, NULL);
            guiGameLoadConfig(selected_item->item->userdata, gameMenuLoadConfig(NULL));
        } else if (menuID == GAME_TEST_CHANGES) {
            guiGameTestSettings(selected_item->item->current->item.id, selected_item->item->userdata, itemConfig);
        } else if (menuID == GAME_REMOVE_CHANGES) {
            if (guiGameShowRemoveSettings(itemConfig, configGetByType(CONFIG_GAME))) {
                menuSetParentalLockCheckState(1); // Re-enable parental lock check.
                guiGameLoadConfig(selected_item->item->userdata, gameMenuLoadConfig(NULL));
            }
        } else if (menuID == GAME_RENAME_GAME) {
            menuRenameGame(&gameMenu);
        } else if (menuID == GAME_DELETE_GAME) {
            menuDeleteGame(&gameMenu);
        }
        // so the exit press wont propagate twice
        readPads();
    }

    if (getKeyOn(KEY_START) || getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE)) {
        guiSwitchScreen(GUI_SCREEN_MAIN);
    }
}

void menuRenderAppMenu()
{
    guiDrawBGPlasma();

    if (!appMenu)
        return;

    // draw the animated menu
    if (!appMenuCurrent)
        appMenuCurrent = appMenu;

    submenu_list_t *it = appMenu;

    // calculate the number of items
    int count = 0;
    int sitem = 0;
    for (; it; count++, it = it->next) {
        if (it == appMenuCurrent)
            sitem = count;
    }

    int spacing = 25;
    int y = (gTheme->usedHeight >> 1) - (spacing * (count >> 1));
    int cp = 0; // current position

    // app title
    fntRenderString(gTheme->fonts[0], 320, 20, ALIGN_CENTER, 0, 0, selected_item->item->current->item.text, gTheme->selTextColor);

    for (it = appMenu; it; it = it->next, cp++) {
        // render, advance
        fntRenderString(gTheme->fonts[0], 320, y, ALIGN_CENTER, 0, 0, submenuItemGetText(&it->item), (cp == sitem) ? gTheme->selTextColor : gTheme->textColor);
        y += spacing;
    }

    // hints
    guiDrawSubMenuHints();
}

void menuHandleInputAppMenu()
{
    if (!appMenu)
        return;

    if (!appMenuCurrent)
        appMenuCurrent = appMenu;

    if (getKey(KEY_UP)) {
        sfxPlay(SFX_CURSOR);
        if (appMenuCurrent->prev)
            appMenuCurrent = appMenuCurrent->prev;
        else // rewind to the last item
            while (appMenuCurrent->next)
                appMenuCurrent = appMenuCurrent->next;
    }

    if (getKey(KEY_DOWN)) {
        sfxPlay(SFX_CURSOR);
        if (appMenuCurrent->next)
            appMenuCurrent = appMenuCurrent->next;
        else
            appMenuCurrent = appMenu;
    }

    if (getKeyOn(gSelectButton)) {
        // execute the item via looking at the id of it
        int menuID = appMenuCurrent->item.id;

        sfxPlay(SFX_CONFIRM);

        if (menuID == 0) {
            menuRenameGame(&appMenu);
        } else if (menuID == 1) {
            menuDeleteGame(&appMenu);
        }
        // so the exit press wont propagate twice
        readPads();
    }

    if (getKeyOn(KEY_START) || getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE)) {
        guiSwitchScreen(GUI_SCREEN_MAIN);
    }
}
