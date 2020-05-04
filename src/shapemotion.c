/** \file shapemotion.c
 *  \brief This is a simple shape motion demo.
 *  This demo creates two layers containing shapes.
 *  One layer contains a rectangle and the other a circle.
 *  While the CPU is running the green LED is on, and
 *  when the screen does not need to be redrawn the CPU
 *  is turned off along with the green LED.
 */  
#include <msp430.h>
#include <libTimer.h>
#include <lcdutils.h>
#include <lcddraw.h>
#include <p2switches.h>
#include <shape.h>
#include <abCircle.h>
#include "buzzer.h"
#include "movement.h"

#define GREEN_LED BIT6


AbRect rect10 = {abRectGetBounds, abRectCheck, {10,10}}; /**< 10x10 rectangle */
AbRArrow rightArrow = {abRArrowGetBounds, abRArrowCheck, 30};

AbRect paddle = {abRectGetBounds, abRectCheck, {3,10}}; //Paddle

AbRectOutline fieldOutline = {	/* playing field */
  abRectOutlineGetBounds, abRectOutlineCheck,   
  {screenWidth/2 - 10, screenHeight/2 - 10}
};
//////////////////////////////////////////////////////////////////////
Layer MyLayer= {
  (AbShape *)&paddle,
  {10, screenHeight/2}, /**< center */
  {0,0}, {0,0},//{15,screenHeight/2}, {screenWidth,screenHeight/2},  /* last & next pos */
  COLOR_BLACK,
  0,
};
Layer MyLayer2= {
  (AbShape *)&paddle,
  {screenWidth-10, screenHeight/2}, /**< center */
  {0,0}, {0,0},//{15,screenHeight/2}, {screenWidth,screenHeight/2},  /* last & next pos */
  COLOR_BLACK,
  &MyLayer,
};
////////////////////////////////////////////////////////////////////////

Layer layer3 = {		/**< Layer with an orange circle */
  (AbShape *)&circle8,
  {(screenWidth/2)+10, (screenHeight/2)+5}, /**< bit below & right of center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_VIOLET,
  &MyLayer2,
};


Layer fieldLayer = {		/* playing field as a layer */
  (AbShape *) &fieldOutline,
  {screenWidth/2, screenHeight/2},/**< center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_BLUE,
  &layer3
};

Layer layer1 = {		/**< Layer with a red square */
  (AbShape *)&rect10,
  {screenWidth/2, screenHeight/2}, /**< center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_RED,
  &fieldLayer,
};

Layer layer0 = {		/**< Layer with an orange circle */
  (AbShape *)&circle14,
  {(screenWidth/2)+10, (screenHeight/2)+5}, /**< bit below & right of center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_ORANGE,
  &layer1,
};

/** Moving Layer
 *  Linked list of layer references
 *  Velocity represents one iteration of change (direction & magnitude)
 */
typedef struct MovLayer_s {
  Layer *layer;
  Vec2 velocity;
  struct MovLayer_s *next;
} MovLayer;

/* initial value of {0,0} will be overwritten */
MovLayer ml5 = { &MyLayer, {0,0}, 0}; /// My figure to test
MovLayer ml6 = { &MyLayer2, {0,0}, 0}; /// My figure to test
MovLayer ml3 = { &layer3, {1,1}, 0}; /**< not all layers move */

MovLayer ml1 = { &layer1, {3,2}, &ml3 }; 
MovLayer ml0 = { &layer0, {-2,3}, &ml1 }; 

static char points[3];

void movLayerDraw(MovLayer *movLayers, Layer *layers)
{
  int row, col;
  MovLayer *movLayer;

  and_sr(~8);			/**< disable interrupts (GIE off) */
  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
    Layer *l = movLayer->layer;
    l->posLast = l->pos;
    l->pos = l->posNext;
  }
  or_sr(8);			/**< disable interrupts (GIE on) */


  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
    Region bounds;
    layerGetBounds(movLayer->layer, &bounds);
    lcd_setArea(bounds.topLeft.axes[0], bounds.topLeft.axes[1], 
		bounds.botRight.axes[0], bounds.botRight.axes[1]);
    for (row = bounds.topLeft.axes[1]; row <= bounds.botRight.axes[1]; row++) {
      for (col = bounds.topLeft.axes[0]; col <= bounds.botRight.axes[0]; col++) {
	Vec2 pixelPos = {col, row};
	u_int color = bgColor;
	Layer *probeLayer;
	for (probeLayer = layers; probeLayer; 
	     probeLayer = probeLayer->next) { /* probe all layers, in order */
	  if (abShapeCheck(probeLayer->abShape, &probeLayer->pos, &pixelPos)) {
	    color = probeLayer->color;
	    break; 
	  } /* if probe check */
	} // for checking all layers at col, row
	lcd_writeColor(color); 
      } // for col
    } // for row
  } // for moving layer being updated
}	  



//Region fence = {{10,30}, {SHORT_EDGE_PIXELS-10, LONG_EDGE_PIXELS-10}}; /**< Create a fence region */

/** Advances a moving shape within a fence
 *  
 *  \param ml The moving shape to be advanced
 *  \param fence The region which will serve as a boundary for ml
 */
void mlAdvance(MovLayer *ml, Region *fence)
{
  Vec2 newPos;
  u_char axis;
  Region shapeBoundary;
  for (; ml; ml = ml->next) {
    vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
    abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);
    for (axis = 0; axis < 2; axis ++) {
      if ((shapeBoundary.topLeft.axes[axis] < fence->topLeft.axes[axis]) ||
	  (shapeBoundary.botRight.axes[axis] > fence->botRight.axes[axis]) ) {
	int velocity = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
	newPos.axes[axis] += (2*velocity);
      }	/**< if outside of fence */
    } /**< for axis */
    ml->layer->posNext = newPos;
  } /**< for ml */
}

void mlBounce(MovLayer *ml, MovLayer *fence, MovLayer *fence2)
{
    Vec2 newPos;
    Vec2 padPos, padPos2;
    u_char axis;
    Region shapeBoundary;
    Region paddleBoundary, paddleBoundary2; // test
    for (; ml; ml = ml->next) {
        vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
        vec2Add(&padPos, &fence->layer->posNext, &fence->velocity); // fence
        vec2Add(&padPos2, &fence2->layer->posNext, &fence2->velocity);
        abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);
        abShapeGetBounds(fence->layer->abShape, &padPos, &paddleBoundary); // fence    
        abShapeGetBounds(fence2->layer->abShape, &padPos2, &paddleBoundary2);
        if ( shapeBoundary.topLeft.axes[0] < paddleBoundary.botRight.axes[0] &&
            (shapeBoundary.botRight.axes[1] > paddleBoundary.topLeft.axes[1] &&
             shapeBoundary.topLeft.axes[1] < paddleBoundary.botRight.axes[1])){
            for (axis = 0; axis < 2; axis ++) {
                int velocity = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
                newPos.axes[axis] += (2*velocity);
            }
            points[0]= points[0]+1;
            buzzer_init(2000);
            ml->layer->posNext = newPos;
        } /**< for ml */
        else if(shapeBoundary.botRight.axes[0] > paddleBoundary2.topLeft.axes[0] &&
            (shapeBoundary.botRight.axes[1] > paddleBoundary2.topLeft.axes[1] &&
            shapeBoundary.topLeft.axes[1] < paddleBoundary2.botRight.axes[1])){
            for (axis = 0; axis < 2; axis ++) {
                int velocity = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
                newPos.axes[axis] += (2*velocity);
            }
            points[2]= points[2]+1;
        buzzer_init(1000);
            ml->layer->posNext = newPos;
        }else {
            buzzer_init(0);
        }
    }
}

u_int bgColor = COLOR_BLUE;     /**< The background color */
int redrawScreen = 1;           /**< Boolean for whether screen needs to be redrawn */

Region fieldFence;		/**< fence around playing field  */


/** Initializes everything, enables interrupts and green LED, 
 *  and handles the rendering for the screen
 */
void game()
{
  points[0]='0';
  points[2]='0';
  points[1]='|';
  int vic=0;

/////////////////////////////////////
    Vec2 padPos;
    vec2Add(&padPos, &ml5.layer->posNext, &ml5.velocity);
    Vec2 padPos2;
    vec2Add(&padPos2, &ml6.layer->posNext, &ml6.velocity);
    ///////////////////////////////////
  for(;;) {
    if(points[0]<'5'&& points[2]<'5'){
        vic=0;
        drawString5x7(screenWidth/2-5,2, points, COLOR_GREEN, COLOR_BLUE);
    //////////////////Reseting buttons
        char str[5];

        while (!redrawScreen) { /**< Pause CPU if screen doesn't need updating */
        P1OUT &= ~GREEN_LED;    /**< Green led off witHo CPU */
        or_sr(0x10);	      /**< CPU OFF */
        }
        P1OUT |= GREEN_LED;       /**< Green led on when CPU on */
        redrawScreen = 0;
        movLayerDraw(&ml0, &layer0);
        movLayerDraw(&ml5, &MyLayer);
        movLayerDraw(&ml6, &MyLayer2);

        u_char width = screenWidth, height = screenHeight;

        u_int switches = p2sw_read(), i;
        
        for (i = 0; i < 4; i++){
            str[i] = (switches & (1<<i)) ? '-' : '1'+i;
        }  
        if(str[0]=='1' && padPos.axes[1]>10){
            int velocity = ml5.velocity.axes[1] = -3;
            padPos.axes[1] += (2*velocity);
        } else if(str[1]=='2' && padPos.axes[1]<(height-10)){
            int velocity = ml5.velocity.axes[1] = 3;
            padPos.axes[1] += (2*velocity);
        }
        
        if(str[2]=='3' && padPos2.axes[1]>10){
            int velocity = ml5.velocity.axes[1] = -3;
            padPos2.axes[1] += (2*velocity);
        } else if(str[3]=='4' && padPos2.axes[1]<(height-10)){
            int velocity = ml5.velocity.axes[1] = 3;
            padPos2.axes[1] += (2*velocity);
        }
        
        //movement();

        str[4] = 0;
        
        ml5.layer->posNext = padPos;
        ml6.layer->posNext = padPos2;
    }else{
        clearScreen(COLOR_BLUE);
        drawString5x7(screenWidth/2,screenHeight/2, "YOU WON", COLOR_BLACK, COLOR_BLUE);
        vic++;
        if(vic==10){
            points[0]='0';
            points[2]='0';
            ml5.layer->posNext.axes[0]=screenWidth/2;
            ml5.layer->posNext.axes[1]=screenHeight/2;
            ml6.layer->posNext.axes[0]=screenWidth/2;
            ml6.layer->posNext.axes[1]=screenHeight/2;
        }
    }
  }
}

void main(){
    P1DIR |= GREEN_LED;		/**< Green led on when CPU on */		
    P1OUT |= GREEN_LED;

    configureClocks();
    lcd_init();
    shapeInit();
    p2sw_init(15);

    shapeInit();

    layerInit(&layer0);
    layerDraw(&layer0);
    layerGetBounds(&fieldLayer, &fieldFence);
    enableWDTInterrupts();      /**< enable periodic interrupt */
    or_sr(0x8);	              /**< GIE (enable interrupts) */
    game();
}

/** Watchdog timer interrupt handler. 15 interrupts/sec */
void wdt_c_handler()
{
  static short count = 0;
  P1OUT |= GREEN_LED;		      /**< Green LED on when cpu on */
  count ++;
  if (count == 20) {
    mlAdvance(&ml0, &fieldFence);
    mlBounce(&ml0, &ml5, &ml6); //test
    if (~p2sw_read())
      redrawScreen = 1;
    count = 0;
  }
  P1OUT &= ~GREEN_LED;		    /**< Green LED off when cpu off */
}
