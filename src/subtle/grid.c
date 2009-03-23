
 /**
  * @package subtle
  *
  * @file Array functions
  * @copyright 2005-2009 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include "subtle.h"

/* Typedef {{{ */
typedef struct gridprops_t
{
  int grav_right;
  int grav_down;
  int cells_x;
  int cells_y;
} GridProps;

static const GridProps gridprops[] =
{
  {0,1, 1,1},

  {0,1, 2,2},
  {0,1, 1,2},
  {1,1, 2,2},

  {0,0, 2,1},
  {0,0, 1,1},
  {1,0, 2,1},

  {0,0, 2,2},
  {0,0, 1,2},
  {1,0, 2,2},
}; /* }}} */

/* GridSlotToRect {{{ */
static void
GridSlotToRect(XRectangle *r,
  XRectangle *slot,
  XRectangle *rect)
{
  rect->x      = slot->x;      // slot->x + w->input.left
  rect->y      = slot->y;      // slot->y + w->input.top;
  rect->width  = slot->width;  // slot->width - (w->input.left + w->input.right);
  rect->height = slot->height; // slot->height - (w->input.top + w->input.bottom);
} /* }}} */

/* GridConstrainSize {{{ */
static void
GridConstrainSize(XRectangle *r,
  XRectangle *slot,
  XRectangle *rect)
{
  //int cw, ch;
  //Rect workarea = { 0, 0, WIDTH, HEIGHT };
  XRectangle sr;

  SlotToRect(r, slot, &sr);

#if 0
  if(constrainNewWindowSize(w, r.width, r.height, &cw, &ch))
    {
      /* constrained size may put window offscreen, adjust for that case */
      int dx = r.x + cw - workarea.width - workarea.x + w->input.right;
      int dy = r.y + ch - workarea.height - workarea.y + w->input.bottom;

      if(dx > 0)
        r.x -= dx;
      if(dy > 0)
        r.y -= dy;

      r.width = cw;
      r.height = ch;
  }
#endif

  *rect = sr;
} /* }}} */

  /** SubGridCommon {{{ 
   * @brief Place rect in grid
   * @param[in]  r     A #XRectangle
   * @param[in]  type  A #SubGrid
   **/

void
SubGrid(XRectangle *r,
  SubGrid type)
{
  XRectangle workarea = { 0, subtle->th, SCREENW, SCREENH - subtle->th };
  XRectangle desiredSlot = { 0 }, desiredRect = { 0 }, currentRect = { 0 };
  GridProps props = gridprops[type];

  assert(r);

  /* slice and dice to get desired slot - including decorations */
  desiredSlot.y      = workarea.y + props.grav_down * (workarea.height / props.xells_y);
  desiredSlot.height = workarea.height / props.cells_y;
  desiredSlot.x      = workarea.x + props.grav_right * (workarea.width / props.cells_x);
  desiredSlot.width  = workarea.width / props.cells_x;

  /* Adjust for constraints and decorations */
  GridConstrainSize(r, &desiredSlot, &desiredRect);

  /* Get current rect not including decorations */
  currentRect.x      = r->x; 
  currentRect.y      = r->y;
  currentRect.width  = r->width;
  currentRect.height = r->height;

	if(desiredRect.y == currentRect.y && desiredRect.height == currentRect.height)
	  {
	    int slotWidth33  = workarea.width / 3;
	    int slotWidth66  = workarea.width - slotWidth33;

      printf("Multi!\n");

	    if(props.numCellsX == 2) /* keys (1, 4, 7, 3, 6, 9) */
	      {
	       if(currentRect.width == desiredRect.width && currentRect.x == desiredRect.x)
	         {
	           desiredSlot.width = slotWidth66;
	           desiredSlot.x = workarea.x + props.gravityRight * slotWidth33;
        		}
      		else
        		{
              Rect rect33, rect66, slot33, slot66;

              slot33       = desiredSlot;
              slot33.x     = workarea.x + props.gravityRight * slotWidth66;
              slot33.width = slotWidth33;
              ConstrainSize(r, &slot33, &rect33);
              DEBUG_RECT(slot33);
              DEBUG_RECT(rect33);

              slot66       = desiredSlot;
              slot66.x     = workarea.x + props.gravityRight * slotWidth33;
              slot66.width = slotWidth66;
              ConstrainSize(r, &slot66, &rect66);
              DEBUG_RECT(slot66);
              DEBUG_RECT(rect66);

              if(currentRect.width == rect66.width && currentRect.x == rect66.x)
                {
                  desiredSlot.width = slotWidth33;
                  desiredSlot.x = workarea.x + props.gravityRight * slotWidth66;
	             }
	         }
	      }
	    else /* keys (2, 5, 8) */
	      {
          if(currentRect.width == desiredRect.width && currentRect.x == desiredRect.x)
            {
              desiredSlot.width = slotWidth33;
              desiredSlot.x     = workarea.x + slotWidth33;
            }
        }

      ConstrainSize(r, &desiredSlot, &desiredRect);
      DEBUG_RECT(desiredRect);
    }

  r->x      = desiredRect.x;
  r->y      = desiredRect.y;
  r->width  = desiredRect.width;
  r->height = desiredRect.height;
} /* }}} */
