/**
  ******************************************************************************
  * @file           : CursorView.h
  * @brief          : Shared OLED cursor overlay (XOR + partial refresh)
  ******************************************************************************
  */
#ifndef __CURSOR_VIEW_H
#define __CURSOR_VIEW_H

void CursorView_Place(void);
void CursorView_Track(void);
void CursorView_Erase(void);

#endif /* __CURSOR_VIEW_H */
