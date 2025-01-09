#ifndef KILO_EDITOR_COMMANDS
#define KILO_EDITOR_COMMANDS
#include "utils.h"

typedef enum EDITOR_COMMANDS{
    ESCAPE,
    DELETE,
    DELETE_LINE,
    YANK,
    YANK_LINE,
    YANK_TO_END_LINE,
    PASTE,
    REPLACE_ONE,
    REPLACE_MANY,
    CHANGE_CASE,
    CHANGE_CASE_UPPER,
    CHANGE_CASE_LOWER,
}EDITOR_COMMANDS;

typedef enum EDITOR_MOTIONS{
    UP,
    DOWN,
    LEFT,
    RIGTH,
    START_LINE,
    END_LINE,
    TOP_FILE,
    BOTTOM_FILE,
    SEARCH_FORWARD,
    SEARCH_BACKWARD,
}EDITOR_MOTIONS;

void editorMoveCursorCommand_(editorConfig *config, int count, EDITOR_MOTIONS motion);
void editorYankCommand_(editorConfig *config, int count, int motion);
void editorPasteCommand_(editorConfig *config, int count, int motion);
void editorSearchCommand_(editorConfig *config, int count, EDITOR_MOTIONS motion, int c);
void editorDeleteCommand_(editorConfig *config, int count, EDITOR_MOTIONS motion);
void editorChangeCaseCommand_(editorConfig *config, int count, EDITOR_MOTIONS motion);
void editorReplaceCommand_(editorConfig *config, int count, EDITOR_MOTIONS motion);

#endif
