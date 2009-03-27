
 /**
  * @package subtle
  *
  * @file Gravity functions
  * @copyright 2005-2009 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include "subtle.h"

/* Typedef {{{ */
typedef struct gravityprops_t
{
  int grav_right;
  int grav_down;
  int cells_x;
  int cells_y;
} GravityProps;
/* }}} */

  /** subGravityCalc {{{ 
   * @brief Calc rect in grid
   * @param[in]  r     A #XRectangle
   * @param[in]  type  A #SubGravity
   **/

void
subGravityCalc(XRectangle *r,
  int type)
{
  XRectangle workarea = { 0, 0, SCREENW, SCREENH - subtle->th };
  XRectangle slot = { 0 }, desired = { 0 }, current = { 0 };

  static const GravityProps props[] =
  {
    { 0, 1, 1, 1 }, ///< Gravity unknown
    { 0, 1, 2, 2 }, ///< Gravity bottom left
    { 0, 1, 1, 2 }, ///< Gravity bottom
    { 1, 1, 2, 2 }, ///< Gravity bottom right
    { 0, 0, 2, 1 }, ///< Gravity left
    { 0, 0, 1, 1 }, ///< Gravity center
    { 1, 0, 2, 1 }, ///< Gravity right
    { 0, 0, 2, 2 }, ///< Gravity top left
    { 0, 0, 1, 2 }, ///< Gravity top
    { 1, 0, 2, 2 }, ///< Gravity top right
  }; 

  assert(r);

  /* Compute slot */
  slot.x      = workarea.x + props[type].grav_right * (workarea.width / props[type].cells_x);
  slot.y      = workarea.y + props[type].grav_down * (workarea.height / props[type].cells_y);
  slot.height = workarea.height / props[type].cells_y;
  slot.width  = workarea.width / props[type].cells_x;

  desired = slot;
  current = *r;

	if(desired.x == current.x && desired.width == current.width)
	  {
	    int height33 = workarea.height / 3;
	    int height66 = workarea.height - height33;
      int comp    = abs(workarea.height - 3 * height33); ///< Int rounding

	    if(2 == props[type].cells_y)
	      {
	       if(current.height == desired.height && current.y == desired.y)
	         {
	           slot.y     = workarea.y + props[type].grav_down * height33;
	           slot.height = height66;
        		}
      		else
        		{
              XRectangle rect33, rect66;

              rect33       = slot;
              rect33.y     = workarea.y + props[type].grav_down * height66;
              rect33.height = height33;

              rect66       = slot;
              rect66.y     = workarea.y + props[type].grav_down * height33;
              rect66.height = height66;

              if(current.height == rect66.height && current.y == rect66.y)
                {
                  slot.height = height33;
                  slot.y     = workarea.y + props[type].grav_down * height66;
	             }
	         }
	      }
	    else
	      {
          if(current.height == desired.height && current.y == desired.y)
            {
              slot.y     = workarea.y + height33;
              slot.height = height33 + comp;
            }
        }

      desired = slot;
    }    

  r->x      = desired.x;
  r->y      = desired.y;
  r->width  = desired.width;
  r->height = desired.height;
} /* }}} */
