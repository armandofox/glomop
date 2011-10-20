import java.awt.*;
import java.net.*;

// The button2 class implements psuedo-buttons for the java_gui.
public class button2 {

// These variables specify the boundaries of this button2.
// The coordinates are calculated from the origin of this button2's
// panel. 
private int x0; // x-origin
private int y0; // y-origin
private int x1;	// x-bottom corner 
private int y1;	// y-botton corner 

// The panel variable specifies which panel this button2 is in.
// This is used in the mouseDown() function of gui.java.  Since
// the coordinates given to mouseDown() are absolute coordinates,
// the panel number needs to be known to specify the relative
// coordinates. These relative coordinates are then passed into
// the inButton() function below.
public int panel;

// "action" contains the action associated with pressing this
// button2. The mouseDown() function in java.gui uses this field 
// to check what kind of action to take in response to this button2 
// being pressed.
public String action;

// "gif" contains the panel that should be displayed when this
// button2 is pressed.
public Image gif;

// "url" is used if the action requires either following a link 
// or setting a preference. This field contains either the link
// or the HTTP request string for setting preferences.
public URL url;

// "mesg" is used to output a status message in the browser's
// dialog box.
public String mesg;

// Default constructor
public button2() {
  x0 = 0;
  y0 = 0;
  x1 = 0;
  y1 = 0;
  panel = 0;
  url = null;
  action = "";
  gif = null;
  mesg = "";
}

// This is the constructor used if no url is specified.  For example, 
// there is no URL needed for minimizing the toolbar.
public button2(int x, int y, int w, int h, int panel0, Image gif0,
	       String action0, String mesg0) {
  x0 = x;
  y0 = y;
  x1 = x + w;
  y1 = y + h;
  panel = panel0;
  action = action0;
  gif = gif0;
  mesg = mesg0;
  url = null;
}

// This is the constructor used for button2's that lead to links or
// setting preferences.  Thus, a URL is specified.
public button2(int x, int y, int w, int h, int panel0, Image gif0, 
	       String action0, String mesg0, String url0) {
  x0 = x;
  y0 = y;
  x1 = x + w;
  y1 = y + h;

  panel = panel0;
  action = action0;
  gif = gif0;
  mesg = mesg0;

  try {
    url = new URL(url0);
  }
  catch (MalformedURLException me) {	// new URL() failed
    System.err.println("new URL failed");
    return;
  }
}

// The basic function used is inButton(x, y) which returns true
// if the coordinate (x, y) lies within this particular button's
// coordinates.
public boolean inButton(int x, int y)  
{
  if (x >= x0 && y >= y0 && x <= x1 && y <= y1) {
    return true;
  }
  return false;
}
}


