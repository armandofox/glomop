import java.applet.*;
import java.awt.*;
import java.net.*;
import java.io.*;

// Things to change for integrating with TranSend:
// - Replace every instance of "gif" with "gif*^i1=1^"
// - Change proxyHost String to "http://transend.CS.Berkeley.EDU/"
// - Take out !!! comments

public class gui extends Applet {

private static AppletContext appletContext;
private static URL codeBase;

// Change the proxyHost if the proxy is relocated.  proxyHost
// is used to specify the server portion of links and set prefs
// requests.
private static String proxyHost = "http://transend.cs.berkeley.edu/";

// A string which holds the status message to be printed out in
// the browser's dialog box.
private String status = "Status: ";	

// enabled is true if the proxy is enabled. This field is used
// so that the applet knows whether to paint the dashboard with
// the current settings or the darkened dashboard.
private static boolean enabled = true;

// Dashboard dimensions
private int height = 453;
private int width = 45;		// Browser width

// Dashboard panels
private int numPanels = 3;	// Number of panels
private static int imgQ = 3;	// Current image quality

// Panel gifs: panels[i] holds the ith panel corresponding to 
// the current settings.  State such as the current image quality
// is stored in this array, but some panels may never change such
// as the links panel.
private Image[] panels;

// Panel gifs for when proxy is disabled. Note that these never
// change.
private Image[] offpanels;
private int[] origins;		// Panel x-origins
private int[] heights;		// Panel heights

// Menu bar buttons
private int numButtons = 13;	// Number of buttons

// An array of button2's.  This array is traversed in the mouseDown()
// function to see whether a mouse click occurred in one of the
// dashboard's button2's.
private button2[] buttons;	

// Buttons that still work when the proxy is disabled.  These are
// used as special cases in the function mouseDown().
private button2 enableButton;

public void init() {

  appletContext = getAppletContext();
  codeBase = getCodeBase();

  width = size().width;

  // Initialize the panel arrays and get the information needed to
  // draw the panels.
  panels = new Image[numPanels];
  offpanels = new Image[numPanels];
  origins = new int[numPanels];
  heights = new int[numPanels];

  int i = 0;
  int origin = 0;
  panels[i] = getImage(codeBase, "enable.gif*^i1=1^");
  offpanels[i] = getImage(codeBase, "disable.gif*^i1=1^");
  heights[i] = 42;
  origins[i] = origin;

  origin += heights[i];
  i += 1;
  panels[i] = getImage(codeBase, "img"+ imgQ + ".gif*^i1=1^");
  offpanels[i] = getImage(codeBase, "img-off.gif*^i1=1^");
  heights[i] = 150;
  origins[i] = origin;

  origin += heights[i];
  i += 1;
  panels[i] = getImage(codeBase, "links.gif*^i1=1^");
  offpanels[i] = getImage(codeBase, "links-off.gif*^i1=1^");
  heights[i] = 261;
  origins[i] = origin;

  // Initialize the button array and get the information needed to
  // associate URLS to buttons.
  buttons = new button2[numButtons];

  int panelnum = 0;
  i = 0;
  buttons[i] = new button2(8, 5, 28, 11, panelnum, 
			   getImage(codeBase, "enable.gif*^i1=1^"),
			   "on-off", "Proxy Enabled", 
			   proxyHost + "SetPrefs/?i1=0");
  enableButton = buttons[i];
  i += 1;
  buttons[i] = new button2(8, 19, 28, 11, panelnum, 
			   getImage(codeBase, "disable.gif*^i1=1^"),
			   "on-off", "Proxy Disabled", 
			   proxyHost + "SetPrefs/?i1=1"); 
  panelnum += 1;
  i += 1;
  buttons[i] = new button2(14, 37, 13, 12, panelnum,
			   getImage(codeBase, "img5.gif*^i1=1^"),
			   "image", "Image Quality Increased", 
			   proxyHost + "SetPrefs/?i5=5");
  i += 1;
  buttons[i] = new button2(15, 54, 11, 11, panelnum,
			   getImage(codeBase, "img5.gif*^i1=1^"),
			   "image", "Image Quality Set", 
			   proxyHost + "SetPrefs/?i5=5");
  i += 1;
  buttons[i] = new button2(15, 68, 11, 11, panelnum, 
			   getImage(codeBase, "img4.gif*^i1=1^"),
			   "image", "Image Quality Set", 
			   proxyHost + "SetPrefs/?i5=4");
  i += 1;
  buttons[i] = new button2(15, 82, 11, 11, panelnum, 
			   getImage(codeBase, "img3.gif*^i1=1^"),
			   "image", "Image Quality Set", 
			   proxyHost + "SetPrefs/?i5=3"); 
  i += 1;
  buttons[i] = new button2(15, 96, 11, 11, panelnum, 
			   getImage(codeBase, "img2.gif*^i1=1^"),
			   "image", "Image Quality Set", 
			   proxyHost + "SetPrefs/?i5=2");
  i += 1;
  buttons[i] = new button2(15, 110, 11, 11, panelnum, 
			   getImage(codeBase, "img1.gif*^i1=1^"),
			   "image", "Image Quality Set", 
			   proxyHost + "SetPrefs/?i5=1");
  i += 1;
  buttons[i] = new button2(14, 126, 13, 12, panelnum,
			   getImage(codeBase, "img1.gif*^i1=1^"),
			   "image", "Image Quality Decreased", 
			   proxyHost + "SetPrefs/?i5=1");
  panelnum += 1;
  i += 1;
  buttons[i] = new button2(3, 6, 36, 58, panelnum, 
			   getImage(codeBase, "links.gif*^i1=1^"),
			   "refine", "Refining All Images",
			   "javascript:Refine()");
  i += 1;
  buttons[i] = new button2(0, 81, 45, 57, panelnum, 
			   getImage(codeBase, "links.gif*^i1=1^"),
			   "link", "Send Comments", 
			   "javascript:Comments()");
  i += 1;
  buttons[i] = new button2(10, 155, 26, 53, panelnum, 
			   getImage(codeBase, "links.gif*^i1=1^"),
			   "link", "Help Page", 
			   "javascript:Help()");
  i += 1;
  buttons[i] = new button2(4, 225, 35, 30, panelnum, 
			   getImage(codeBase, "links.gif*^i1=1^"),
			   "link", "About Page", 
			   "javascript:About()");

  /* !!!
*/
  // Each time init is called, we must poll the proxy for the current
  // user settings -- image quality and proxy enabled/disabled.
  try {
    URL prefsURL = new URL(proxyHost + "GetPrefs.html");
    URLConnection prefsConnection = prefsURL.openConnection();
    DataInputStream prefsStream = new DataInputStream(prefsConnection.getInputStream());
    String inputLine;
    int mySetting;

    inputLine = prefsStream.readLine();
    if (inputLine.startsWith("true")) {
      enabled = true;
    }
    else if (inputLine.startsWith("false")) {
      enabled = false;
    }

    inputLine = prefsStream.readLine();
    imgQ = Integer.parseInt(inputLine);

    prefsStream.close();
  }
  catch (MalformedURLException me) {
    System.err.println("new URL failed");
    return;
  }
  catch (IOException ioex) {
    System.err.println("IO exception");
    return;
  }
/*
  !! */

  // Find out the current state of the image quality preference and 
  // select the right panel.
  panels[1] = getImage(codeBase, "img"+ imgQ + ".gif*^i1=1^");
}

public void paint(Graphics g) {
  // If the proxy is currently disabled, we use the offpanels array
  // to draw the dashboard.  This results in the darkened dashboard.
  int i;
  if (!enabled) {
    for (i = 0; i < numPanels; i += 1) {
      g.drawImage(offpanels[i], 0, origins[i], this);
    }
  }     
  // Otherwise, the panel is not minmized and the proxy is enabled,
  // so use the panels array to paint the dashboard.
  else {
    for (i = 0; i < numPanels; i += 1) {
      g.drawImage(panels[i], 0, origins[i], this);
    }
  }
  // This paints the last panel flush right.
  /*
  g.setColor(Color.black);
  g.fillRect(origins[i-1]+heights[i-1], 0, origins[i], height);
  g.drawImage(offpanels[i], origins[i], 0, this);
  */
}

// Repaints panel number "panelNum"
public void repaintPanel(int panelNum) {
  repaint(0, origins[panelNum], width, heights[panelNum]);
}

public boolean mouseDown(Event e, int x, int y) {

  // If the proxy is enabled, go through each button2 to
  // see if the click occurred in that button2.  Do the same if
  // the proxy is disabled but the click occurs in one of the button2's
  // that can still be pressed while disabled; e.g. enableButton and
  // minButton.
  button2 b;
  String action = "";
  String mesg = "";

  if (enabled || enableButton.inButton(x, y - origins[enableButton.panel])) {
  for (int i = 0; i < numButtons; i += 1) {

    b = buttons[i];
    int p = b.panel;

    if (b.inButton(x, y - origins[p])) {

      action = b.action;
      mesg = b.mesg;
      status = "Status: " + b.mesg;

      // If the buttton action equals "link" or "refine" we can call
      // showDocument to get the desired behavior.
      if (action.equals("link") || action.equals("refine")) {
        appletContext.showDocument(b.url);
	appletContext.showStatus(status);
        return false;
      }

      // If we are setting a preference:
      else if ((action.equals("on-off")) || (action.equals("image"))) {

        // Check if we are setting the image quality explicitly.  
	// This is a HACK!!
        if (mesg.equals("Image Quality Set")) {	
          imgQ = 8 - i;
        }
	else if (mesg.equals("Image Quality Increased")) {
	  if (imgQ < 5) {
	    imgQ += 1;
	  }
	  b = buttons[8-imgQ];
	}
	else if (mesg.equals("Image Quality Decreased")) {
	  if (imgQ > 1) {
	    imgQ -= 1;
	  }
	  b = buttons[8-imgQ];
	}

	/* !!!
*/
        // Next we send a GET request to the proxy to set this preference.
        try {
          b.url.openConnection();
        }
        catch (IOException ioex) {
          System.err.println("Open Connection failed");
          return false;
        }
        try {
          DataInputStream prefsStream = new DataInputStream(b.url.openStream());
          String inputLine;
          while((inputLine = prefsStream.readLine()) != null) {
          }
          prefsStream.close();
        }
        catch (IOException ioex) {
          System.err.println("Open Stream failed");
          return false;
        }
/*
	!!! */

        // If we are enabling or disabling the proxy, we have to set
        // some state variables and repaint the entire applet.
        panels[p] = b.gif;
        if (mesg.equals("Proxy Enabled")) {
          enabled = true;
          repaint();
        }
        else if (mesg.equals("Proxy Disabled")) {
          enabled = false;
          repaint();
        }

        // Otherwise, we can just repaint one panel.
        else {
          repaintPanel(p);
        }
      }
    }	
  }
  }
  appletContext.showStatus(status);
  return false;
}
}


