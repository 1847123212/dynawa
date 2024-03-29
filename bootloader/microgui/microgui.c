/* ========================================================================== */
/*                                                                            */
/*   Filename.c                                                               */
/*   (c) 2001 Author                                                          */
/*                                                                            */
/*   Description                                                              */
/*                                                                            */
/* ========================================================================== */

#include "scroutput.h"

#ifdef UGD_STDOUT
#include <stdio.h>
#include <stdint.h>

#else
#include <unistd.h>
//#include <stdlib.h>
#include "hardware_conf.h"
#include "firmware_conf.h"
#include <screen/screen.h>
#include <debug/trace.h>
#endif

#include "ugscrdrv.h"
#include "microgui.h"

//each component has only one associted function, several events are applicable
//
// mandatory events:
//   UGUI_EV_REDRAW
//

//void uguiBtn(tugui_Btn *btn, tugui_Event evt);

tugui_Win *wins[UGUI_MAXWINS];
tugui_Context wini=0;
tugui_Context winn=0;
tugui_Context winlast=0;
tugui_Event evtbuf[(1<<UGUI_EVT_BUF_LEN)];
uint8_t evtbri, evtbwi;

// for
// tugui_Plain
//  declare YOUR OWN handler!

//void uguiBtn(tugui_Btn *btn, tugui_Event evt)
//tugui_Event uguiBtn(void *s, tugui_Event evt)
tugui_Event uguiBtn(tugui_Btn *btn, tugui_Event evt)
{
   // tugui_Btn *btn=(tugui_Btn *)s;
   TRACE_ALL("[uguiBtn%d]",evt);
   switch (evt)
   {
     //mandatory events:
     case UGUI_EV_REDRAW:
     {
       ugdbgColor = btn->bgColor;
       ugd_rect(btn->x, btn->y, btn->x+btn->w, btn->y+btn->h-1);
       if (btn->bitmap!=NULL) ugd_bitmap(btn->x, btn->y, btn->bitmapw, btn->bitmaph, btn->bitmap);
       if (btn->label!=NULL) 
       {
         ugdfgColor = btn->fgColor;
         ugdbgColor = btn->bgColor;
         ugdfont = btn->font;              
         ugd_print(btn->x+btn->bitmapw, btn->y+btn->h/2-ugd_charh()/2,btn->label);
       }
       break;
     }
     //user events:
     default:
     {
        if (evt==btn->pushEvent)
        {
           //proceed push event:
           if (btn->label!=NULL) 
           {
             ugdfgColor = btn->fgColorPushed;
             ugdbgColor = btn->bgColorPushed;
             ugdfont = btn->font;   
             ugd_rect(btn->x, btn->y, btn->x+btn->w, btn->y+btn->h-1);
             ugd_print(btn->x+btn->bitmapw, btn->y+btn->h/2-ugd_charh()/2,btn->label);
           }
           if (btn->pushCallback!=NULL) btn->pushCallback(NULL);                      
        }
        if (evt==btn->releaseEvent)
        {
           //proceed release event:
           if (btn->label!=NULL) 
           {
             ugdfgColor = btn->fgColor;
             ugdbgColor = btn->bgColor;
             ugdfont = btn->font;
             ugd_rect(btn->x, btn->y, btn->x+btn->w, btn->y+btn->h-1);
             ugd_print(btn->x+btn->bitmapw, btn->y+btn->h/2-ugd_charh()/2,btn->label);
           }
           if (btn->releaseCallback!=NULL) btn->releaseCallback(NULL);                    
        }
        break;
     }      
   }
   return 0;
}

tugui_Event uguiImage(tugui_Image *img, tugui_Event evt)
{
  TRACE_ALL("[uguiImage%d]",evt);
   switch (evt)
   {
     //mandatory events:
     case UGUI_EV_REDRAW:
     {        
       ugd_bitmap(img->x, img->y, img->w, img->h, img->bitmap);       
       break;
     }
     //user events:
     default:
     {
        
        break;
     }      
   }   
   return 0;
}

//tugui_Event uguiLabel(void *s, tugui_Event evt)
tugui_Event uguiLabel(tugui_Label *lab, tugui_Event evt)
{
  TRACE_ALL("[uguiLabel%d]",evt);
   //tugui_Label *lab=(tugui_Label*)s;
   switch (evt)
   {
     //mandatory events:
     case UGUI_EV_REDRAW:
     {        
         ugdfgColor = lab->fgColor;
         ugdbgColor = lab->bgColor;
         ugd_rect(lab->x, lab->y, lab->x+lab->w-1,lab->y+lab->h-1);
         ugdfont = lab->font;     
         ugd_print(lab->x, lab->y,lab->label);            
       break;
     }
     //user events:
     default:
     {
        
        break;
     }      
   }
   
   return 0;
}

tugui_Event uguiRuller(tugui_Ruller *rul, tugui_Event evt)
{   
   TRACE_ALL("[uguiRuller%d]",evt);
   switch (evt)
   {
     //mandatory events:
     case UGUI_EV_REDRAW:
     { 
       ugdbgColor = rul->fgColor; //rect uses backgroud color
       if (rul->vert)               
         ugd_rect(rul->x, rul->y,rul->x+rul->thick-1,rul->y+rul->len-1);
       else
         ugd_rect(rul->x, rul->y,rul->x+rul->len-1,rul->y+rul->thick-1);    
       break;
     }
     //user events:
     default:
     {
        
        break;
     }      
   }
   
   return 0;
}

tugui_Event uguiListbox(tugui_Listbox *box, tugui_Event evt)
{  
   tugui_ScrCoord itofsx, itofsy;
   
   uint8_t i,iofs,iend;
   
   void itemprint(void)
   {
     //NOTE: iofs encountered!
     if (box->flow) { itofsx=box->itemw*(box->itemi-iofs); itofsy=0; } else { itofsx=0; itofsy=box->itemh*(box->itemi-iofs); };
     //check box boundary:
      //if (itofsx>box->w) itofsx=
     
     ugdfont = box->font;    
     ugd_rect(box->x+itofsx, box->y+itofsy, box->x+itofsx+box->itemw, box->y+itofsy+box->itemh-1);
     ugd_print(box->x+itofsx+1, box->y+itofsy, box->items[box->itemi]);    
     if (box->props!=NULL)
     {
       if (box->flow) itofsy+=box->itemh+1; else itofsx+=box->itemw+1;
       ugd_rect(box->x+itofsx, box->y+itofsy, box->x+itofsx+box->propw, box->y+itofsy+box->proph-1);
       ugd_print(box->x+itofsx, box->y+itofsy, box->props[box->itemi]); 
     }   
   }
   
   void redraw(void)
   {
     //x or y layout
       iofs=0;
       if (box->flow) {if (box->itemi>(box->w-1)) iofs=box->itemi-(box->w-1); iend=box->w;} else {if (box->itemi>(box->h-1)) iofs=box->itemi-(box->h-1); iend=box->h;};
       ugdfont = box->font;    
       for (i=0;i<iend;i++)
       {
          if ((i+iofs)>=box->itemn) break;
          if (box->flow) { itofsx=box->itemw*i; itofsy=0; } else { itofsx=0; itofsy=box->itemh*i; };
          //check box boundary:
          //if (itofsx>box->w) itofsx=
          if ((i+iofs)==(box->itemi))
          {
            ugdfgColor = box->fgColorSelected;
            ugdbgColor = box->bgColorSelected;
          } else {
            ugdfgColor = box->fgColor;
            ugdbgColor = box->bgColor;
          }
          ugd_rect(box->x+itofsx, box->y+itofsy, box->x+itofsx+box->itemw, box->y+itofsy+box->itemh-1);
          ugd_print(box->x+itofsx+1, box->y+itofsy, box->items[i+iofs]); 
          if (box->props!=NULL)
          {
             if (box->flow) itofsy+=box->itemh+1; else itofsx+=box->itemw+1;
             ugd_rect(box->x+itofsx, box->y+itofsy, box->x+itofsx+box->propw, box->y+itofsy+box->proph-1);
             ugd_print(box->x+itofsx, box->y+itofsy, box->props[i+iofs]); 
          }      
       }         
   }
   
   TRACE_ALL("[uguiListbox%d]",evt);
   switch (evt)
   {
     //mandatory events:
     case UGUI_EV_REDRAW:
     { 
       ugdfgColor = box->fgColor;
       ugdbgColor = box->bgColor;
        
       ugd_rect(box->x, box->y, box->x+(box->w*box->itemw), box->y+(box->h*box->itemh));    
       if (box->label!=NULL) ugd_print(box->x, box->y, box->label);
       
       redraw();
       break;
     }
     //user events:
     default:
     {
        if (evt==box->selectEvent)
        {
           //proceed select event:
           if (box->selectCallback!=NULL) box->selectCallback(NULL);                      
        }
        
        if ((evt==box->scrollDnEvent)||(evt==box->scrollUpEvent))
        {
          ugdfgColor = box->fgColor;
          ugdbgColor = box->bgColor;
          iofs=0;
          if (box->flow) {if (box->itemi>(box->w-1)) iofs=box->itemi-(box->w-1); } else {if (box->itemi>(box->h-1)) iofs=box->itemi-(box->h-1); };
          if (!iofs) itemprint(); //cleanup old selected
          if (evt==box->scrollDnEvent)
          {
             //proceed scroll down event:
             if ((box->itemi+1)<(box->itemn)) 
             { 
               box->itemi++;           
               if (box->scrollDnCallback!=NULL) box->scrollDnCallback(&(box->itemi));               
             }                                             
          }
          
          if (evt==box->scrollUpEvent)
          {
             //proceed scroll up event:
             if (box->itemi>0) 
             {
               box->itemi--;
               if (box->scrollUpCallback!=NULL) box->scrollUpCallback(&(box->itemi));                
             }                          
          }
          ugdfgColor = box->fgColorSelected;
          ugdbgColor = box->bgColorSelected;
          if (box->flow) {if (box->itemi>(box->w-1)) iofs=box->itemi-(box->w-1);  } else {if (box->itemi>(box->h-1)) iofs=box->itemi-(box->h-1);};
          if (iofs) { redraw(); } else itemprint();
          
            //highlight selected                              
        }
        break;
     }      
   }
   
   return;
}

tugui_Event uguiBargraph(tugui_Bargraph *bar, tugui_Event evt)
{
  int16_t v;
  // tugui_Btn *btn=(tugui_Btn *)s;
  TRACE_ALL("[uguiBar%d]",evt);
  switch (evt)
  {
    //mandatory events:
    case UGUI_EV_REDRAW:
    {
      ugdbgColor = bar->bgColor;//clr scr
      ugd_rect(bar->x, bar->y, bar->x+bar->w, bar->y-bar->h);
      //min max |
      ugdbgColor = bar->fgColor;
      if (bar->vert)
      {
        ugd_rect(bar->x, bar->y, bar->x+bar->w, bar->y);
        ugd_rect(bar->x, bar->y-bar->h, bar->x+bar->w, bar->y-bar->h);
        ugd_rect(bar->x, bar->y-bar->h/2, bar->x+bar->w, bar->y-bar->h/2);
        if (bar->mirror)
          ugd_rect(bar->x+bar->w, bar->y, bar->x+bar->w, bar->y-bar->h);
        else
          ugd_rect(bar->x, bar->y, bar->x, bar->y-bar->h);
      } else {
        ugd_rect(bar->x, bar->y, bar->x, bar->y-bar->h);
        ugd_rect(bar->x+bar->w, bar->y, bar->x+bar->w, bar->y-bar->h);
        ugd_rect(bar->x+bar->w/2, bar->y, bar->x+bar->w/2, bar->y-bar->h);
        ugd_rect(bar->x, bar->y, bar->x+bar->w, bar->y);
      }
      
      v= bar->value-bar->min; //offset cancel
      if (v<0) v=0;
      //scale:
      if (bar->vert)
        v= (v*bar->h)/(bar->max-bar->min);
      else
        v= (v*bar->w)/(bar->max-bar->min);
      
      //
      ugdbgColor = bar->fgColor;
      if (bar->vert)
      {
        if (bar->mirror)
          ugd_rect(bar->x+1, bar->y-v, bar->x+bar->w, bar->y);
        else  
          ugd_rect(bar->x, bar->y-v, bar->x+bar->w-1, bar->y);
      }  
      else {
        ugd_rect(bar->x, bar->y, bar->x+v, bar->y-bar->h+1);
      }    
      break;
    }
  }
}     

//======================

tugui_Event uguiWin(tugui_Win *win, tugui_Event evt)
{
   //tugui_Label *lab=(tugui_Label*)s;
   uint8_t i;
   TRACE_ALL("[uguiWin%d]",evt);
   switch (evt)
   {
     //mandatory events:
     case UGUI_EV_REDRAW:
     {
       ugdbgColor = win->bgColor;
       ugd_rect(win->x, win->y, win->x+win->w, win->y+win->h);
       if (win->attr&UGUI_WINATTR_DRAWFRAME)
       {
         ugdbgColor = win->fgColor;
         ugd_rect(win->x, win->y, win->x+win->w, win->y);
         ugd_rect(win->x, win->y, win->x, win->y+win->h);
         ugd_rect(win->x+win->w, win->y, win->x+win->w, win->y+win->h);
         ugd_rect(win->x, win->y+win->h, win->x+win->w, win->y+win->h);
       }
       break;
     }
     //user events:
     default:
     {
        
        break;
     }      
   }
   //send event to all objects:
   for (i=0;i<win->objn;i++)
   {
     win->objs[i].evthandler(win->objs[i].obj,evt);
     //TRACE_ALL(">");
     //printf("*");
   }
   return 0;
}

tugui_Err uguiWinAddObj(tugui_Win *win,  void* obj,   tugui_Event (*evthandler)(const void *, tugui_Event))
{
  if (win->objn>UGUI_MAXWINOBJS-1) return 1;
  win->objs[win->objn].obj=obj;
  win->objs[win->objn].evthandler=evthandler;
  
  win->objn++;
  
  TRACE_ALL("[objadd]");
  return 0;
}

void uguiWriteEvt(tugui_Event evt)
{
  //circular buffer:
  evtbuf[evtbwi] = evt;  
  //printf("uguiWriteEvt:%d@%d ",evt,evtbwi);  
  evtbwi++;
  evtbwi&=((1<<UGUI_EVT_BUF_LEN)-1);

}

tugui_Event uguiReadEvt(void)
{
  tugui_Event r;
  if (evtbwi==evtbri) return 0;
  r = evtbuf[evtbri]; 
  //printf("uguiReadEvt:%d@%d ",r,evtbri);
  evtbri++;
  evtbri&=((1<<UGUI_EVT_BUF_LEN)-1);  
  return r;
}


tugui_Err uguiContextAddWin(tugui_Win *win)
{
  if (winn>=(UGUI_MAXWINS-1)) return 1;
  wins[winn]=win;
  winn++;  
  TRACE_ALL("[winadd]");
  return 0;
}

tugui_Err  uguiContextSwitch(tugui_Context c)
{
   if ((c>=0)&&(c<winn)) 
   { 
     winlast=wini; //should be stack 
     wini=c;      
     uguiWriteEvt(UGUI_EV_REDRAW);      
  }
   TRACE_ALL(".CX.");
   return 0;
}

//this should be STACK!
tugui_Context uguiLastContext(void)
{
  return winlast;
}

tugui_Context uguiContext(void)
{
  return wini;
}

// container of WINs and context switch of events
void uguiContextWrapper(void)
{
  tugui_Event evt;
  if (evt=uguiReadEvt()) uguiWin(wins[wini],evt);
}

void uguiInitContext(void)
{
  wini=0;
  winn=0;
  winlast=0;
   //evtbuf[(1<<UGUI_EVT_BUF_LEN)];
  evtbri=0;
  evtbwi=0;
}

//??tugui_Err immediateRefresh()
