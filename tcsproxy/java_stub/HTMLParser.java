import java.io.*;
import java.util.*;

class HTMLElement
{
  public String name;
  public String[] attributes;
  public int[] model;
  public int content;
  public boolean startTagOptional;
  public boolean endTagOptional;

  public HTMLElement(String n, String[] attr, int[] m, int c,
                     boolean start, boolean end)
  {
    name = n;
    attributes = attr;
    model = m;
    content = c;
    startTagOptional = start;
    endTagOptional = end;
  }
}

class HTMLParser
{
  private final static int TT_EOF = -1;
  private final static int TT_START_TAG = -2;
  private final static int TT_END_TAG = -3;
  private final static int TT_TEXT = -4;
  private final static int TT_NONE = -5;

  private final static int HTML_A = 0;
  private final static int HTML_ADDRESS = 1;
  private final static int HTML_APPLET = 2;
  private final static int HTML_AREA = 3;
  private final static int HTML_B = 4;
  private final static int HTML_BASE = 5;
  private final static int HTML_BIG = 6;
  private final static int HTML_BLOCKQUOTE = 7;
  private final static int HTML_BODY = 8;
  private final static int HTML_BR = 9;
  private final static int HTML_CAPTION = 10;
  private final static int HTML_CENTER = 11;
  private final static int HTML_CITE = 12;
  private final static int HTML_CODE = 13;
  private final static int HTML_DD = 14;
  private final static int HTML_DFN = 15;
  private final static int HTML_DIR = 16;
  private final static int HTML_DIV = 17;
  private final static int HTML_DL = 18;
  private final static int HTML_DT = 19;
  private final static int HTML_EM = 20;
  private final static int HTML_FONT = 21;
  private final static int HTML_FORM = 22;
  private final static int HTML_H1 = 23;
  private final static int HTML_H2 = 24;
  private final static int HTML_H3 = 25;
  private final static int HTML_H4 = 26;
  private final static int HTML_H5 = 27;
  private final static int HTML_H6 = 28;
  private final static int HTML_HEAD = 29;
  private final static int HTML_HR = 30;
  private final static int HTML_HTML = 31;
  private final static int HTML_I = 32;
  private final static int HTML_IMG = 33;
  private final static int HTML_INPUT = 34;
  private final static int HTML_ISINDEX = 35;
  private final static int HTML_KBD = 36;
  private final static int HTML_LI = 37;
  private final static int HTML_LINK = 38;
  private final static int HTML_MAP = 39;
  private final static int HTML_MENU = 40;
  private final static int HTML_META = 41;
  private final static int HTML_OL = 42;
  private final static int HTML_OPTION = 43;
  private final static int HTML_P = 44;
  private final static int HTML_PARAM = 45;
  private final static int HTML_PRE = 46;
  private final static int HTML_SAMP = 47;
  private final static int HTML_SCRIPT = 48;
  private final static int HTML_SELECT = 49;
  private final static int HTML_SMALL = 50;
  private final static int HTML_STRIKE = 51;
  private final static int HTML_STRONG = 52;
  private final static int HTML_STYLE = 53;
  private final static int HTML_SUB = 54;
  private final static int HTML_SUP = 55;
  private final static int HTML_TABLE = 56;
  private final static int HTML_TD = 57;
  private final static int HTML_TEXTAREA = 58;
  private final static int HTML_TH = 59;
  private final static int HTML_TITLE = 60;
  private final static int HTML_TR = 61;
  private final static int HTML_TT = 62;
  private final static int HTML_U = 63;
  private final static int HTML_UL = 64;
  private final static int HTML_VAR = 65;
  private final static int HTML_DOCUMENT = 66;

  private final static String[] noAttr = {
  };
  private final static String[] aAttr = {
    "HREF", "NAME", "REL", "TITLE"
  };
  private final static String[] appletAttr = {   
    "ALT", "ALIGN", "CODE", "CODEBASE", "HEIGHT", "HSPACE", "NAME", "VSPACE",
    "WIDTH"
  };
  private final static String[] areaAttr = {   
    "ALT", "COORDS", "HREF", "NOHREF", "SHAPE"
  };
  private final static String[] baseAttr = {
    "HREF"
  };      
  private final static String[] bodyAttr = {
    "ALINK", "BACKGROUND", "BGCOLOR", "LINK", "TEXT", "VLINK"
  };      
  private final static String[] brAttr = {
    "CLEAR"
  };
  private final static String[] alignAttr = {
    "ALIGN"
  };
  private final static String[] compactAttr = {
    "COMPACT"
  };
  private final static String[] fontAttr = {       
    "COLOR", "SIZE"
  };
  private final static String[] formAttr = {       
    "ACTION", "ENCTYPE", "METHOD"
  };
  private final static String[] hrAttr = {              
    "ALIGN", "NOSHADE", "SIZE", "WIDTH"
  };
  private final static String[] htmlAttr = {              
    "VERSION"
  };
  private final static String[] imgAttr = { 
    "ALIGN", "ALT", "BORDER", "HEIGHT", "HSPACE", "ISMAP", "SRC", "USEMAP",
    "VSPACE", "WIDTH"
  };      
  private final static String[] inputAttr = {
    "ALIGN", "CHECKED", "MAXLENGTH", "NAME", "SIZE", "SRC", "TYPE", "VALUE"
  };
  private final static String[] isindexAttr = {
    "PROMPT"
  };
  private final static String[] liAttr = {
    "TYPE", "VALUE"
  };
  private final static String[] linkAttr = {
    "HREF", "ID", "REL", "REV", "TITLE"
  };      
  private final static String[] mapAttr = {
    "NAME"
  };
  private final static String[] metaAttr = {
    "CONTENT", "HTTP-EQUIV", "NAME"
  };
  private final static String[] olAttr = {
    "COMPACT", "START", "TYPE"
  };
  private final static String[] optionAttr = {
    "SELECTED", "VALUE"
  };
  private final static String[] paramAttr = {
    "NAME", "VALUE"
  };
  private final static String[] preAttr = {
    "WIDTH"
  };
  private final static String[] selectAttr = {
    "MULTIPLE", "NAME", "SIZE"
  };
  private final static String[] tableAttr = {
    "ALIGN", "BORDER", "CELLPADDING", "CELLSPACING", "DUMMY", "WIDTH"
  };
  private final static String[] thtdAttr = {
    "ALIGN", "COLSPAN", "HEIGHT", "NOWRAP", "ROWSPAN", "VALIGN", "WIDTH"
  };
  private final static String[] textareaAttr = {
    "COLS", "NAME", "ROWS"
  };
  private final static String[] trAttr = {
    "ALIGN", "VALIGN"
  };
  private final static String[] ulAttr = {
    "COMPACT", "TYPE"
  };

  private final static int[] noModel = {
  };
  private final static int[] textModel = {
    HTML_TT, HTML_I, HTML_B, HTML_U, HTML_STRIKE, HTML_BIG, HTML_SMALL,
    HTML_SUB, HTML_SUP, 
    HTML_EM, HTML_STRONG, HTML_DFN, HTML_CODE, HTML_SAMP, HTML_KBD, HTML_VAR,
    HTML_CITE,
    HTML_A, HTML_IMG, HTML_APPLET, HTML_FONT, HTML_BR, HTML_SCRIPT, HTML_MAP,
    HTML_INPUT, HTML_SELECT, HTML_TEXTAREA
  };
  private final static int[] addressModel = {
    HTML_TT, HTML_I, HTML_B, HTML_U, HTML_STRIKE, HTML_BIG, HTML_SMALL,
    HTML_SUB, HTML_SUP, 
    HTML_EM, HTML_STRONG, HTML_DFN, HTML_CODE, HTML_SAMP, HTML_KBD, HTML_VAR,
    HTML_CITE,
    HTML_A, HTML_IMG, HTML_APPLET, HTML_FONT, HTML_BR, HTML_SCRIPT, HTML_MAP,
    HTML_INPUT, HTML_SELECT, HTML_TEXTAREA,
    HTML_P
  };
  private final static int[] appletModel = {
    // %text
    HTML_TT, HTML_I, HTML_B, HTML_U, HTML_STRIKE, HTML_BIG, HTML_SMALL,
    HTML_SUB, HTML_SUP, 
    HTML_EM, HTML_STRONG, HTML_DFN, HTML_CODE, HTML_SAMP, HTML_KBD, HTML_VAR,
    HTML_CITE,
    HTML_A, HTML_IMG, HTML_APPLET, HTML_FONT, HTML_BR, HTML_SCRIPT, HTML_MAP,
    HTML_INPUT, HTML_SELECT, HTML_TEXTAREA,
    HTML_PARAM
  };
  private final static int[] bodyModel = {
    // %heading
    HTML_H1, HTML_H2, HTML_H3, HTML_H4, HTML_H5, HTML_H6,
    // %text
    HTML_TT, HTML_I, HTML_B, HTML_U, HTML_STRIKE, HTML_BIG, HTML_SMALL,
    HTML_SUB, HTML_SUP, 
    HTML_EM, HTML_STRONG, HTML_DFN, HTML_CODE, HTML_SAMP, HTML_KBD, HTML_VAR,
    HTML_CITE,
    HTML_A, HTML_IMG, HTML_APPLET, HTML_FONT, HTML_BR, HTML_SCRIPT, HTML_MAP,
    HTML_INPUT, HTML_SELECT, HTML_TEXTAREA, 
    // %block
    HTML_P, HTML_UL, HTML_OL, HTML_DIR, HTML_MENU, HTML_PRE, HTML_DL, 
    HTML_DIV, HTML_CENTER, HTML_BLOCKQUOTE, HTML_FORM, HTML_ISINDEX, HTML_HR,
    HTML_TABLE,
    HTML_ADDRESS
  };
  private final static int[] flowModel = {
    // %text
    HTML_TT, HTML_I, HTML_B, HTML_U, HTML_STRIKE, HTML_BIG, HTML_SMALL,
    HTML_SUB, HTML_SUP, 
    HTML_EM, HTML_STRONG, HTML_DFN, HTML_CODE, HTML_SAMP, HTML_KBD, HTML_VAR,
    HTML_CITE,
    HTML_A, HTML_IMG, HTML_APPLET, HTML_FONT, HTML_BR, HTML_SCRIPT, HTML_MAP,
    HTML_INPUT, HTML_SELECT, HTML_TEXTAREA, 
    // %block
    HTML_P, HTML_UL, HTML_OL, HTML_DIR, HTML_MENU, HTML_PRE, HTML_DL, 
    HTML_DIV, HTML_CENTER, HTML_BLOCKQUOTE, HTML_FORM, HTML_ISINDEX, HTML_HR,
    HTML_TABLE
  };
  private final static int[] liModel = {
    HTML_LI
  };
  private final static int[] dlModel = {
    HTML_DT, HTML_DD
  };
  private final static int[] headModel = {
    HTML_TITLE, HTML_ISINDEX, HTML_BASE,
    HTML_SCRIPT, HTML_STYLE, HTML_META, HTML_LINK
  };
  private final static int[] htmlModel = {
    HTML_HEAD, HTML_BODY
  };
  private final static int[] mapModel = {
    HTML_AREA
  };
  private final static int[] selectModel = {
    HTML_OPTION
  };
  private final static int[] tableModel = {
    HTML_CAPTION, HTML_TR
  };
  private final static int[] trModel = {
    HTML_TH, HTML_TD
  };
  private final static int[] documentModel = {
    HTML_HTML
  };

  private final static int ELEMENT_CONTENT = -1;
  private final static int MIXED_CONTENT = -2;
  private final static int LITERAL_CONTENT = -3;

  private final static HTMLElement[] elements = {
    new HTMLElement("A", aAttr, textModel, MIXED_CONTENT, false, false),
    new HTMLElement("ADDRESS", noAttr, addressModel, MIXED_CONTENT, false,
                    false),
    new HTMLElement("APPLET", appletAttr, appletModel, ELEMENT_CONTENT, false,
                    false),
    new HTMLElement("AREA", areaAttr, noModel, ELEMENT_CONTENT, false, true),
    new HTMLElement("B", noAttr, textModel, MIXED_CONTENT, false, false),
    new HTMLElement("BASE", baseAttr, noModel, ELEMENT_CONTENT, false, true),
    new HTMLElement("BIG", noAttr, textModel, MIXED_CONTENT, false, false),
    new HTMLElement("BLOCKQUOTE", noAttr, bodyModel, MIXED_CONTENT, false,
                    false),
    new HTMLElement("BODY", bodyAttr, bodyModel, MIXED_CONTENT, true, true),
    new HTMLElement("BR", brAttr, noModel, ELEMENT_CONTENT, false, true),
    new HTMLElement("CAPTION", alignAttr, textModel, MIXED_CONTENT, false,
                    false),
    new HTMLElement("CENTER", noAttr, bodyModel, MIXED_CONTENT, false, false),
    new HTMLElement("CITE", noAttr, textModel, MIXED_CONTENT, false, false),
    new HTMLElement("CODE", noAttr, textModel, MIXED_CONTENT, false, false),
    new HTMLElement("DD", noAttr, flowModel, MIXED_CONTENT, false, true),
    new HTMLElement("DFN", noAttr, textModel, MIXED_CONTENT, false, false),
    new HTMLElement("DIR", compactAttr, liModel, ELEMENT_CONTENT, false,
                    false),
    new HTMLElement("DIV", alignAttr, bodyModel, MIXED_CONTENT, false, false),
    new HTMLElement("DL", compactAttr, dlModel, ELEMENT_CONTENT, false, false),
    new HTMLElement("DT", noAttr, textModel, MIXED_CONTENT, false, true),
    new HTMLElement("EM", noAttr, textModel, MIXED_CONTENT, false, false),
    new HTMLElement("FONT", fontAttr, textModel, MIXED_CONTENT, false, false),
    new HTMLElement("FORM", formAttr, bodyModel, MIXED_CONTENT, false, false),
    new HTMLElement("H1", alignAttr, textModel, MIXED_CONTENT, false, false),
    new HTMLElement("H2", alignAttr, textModel, MIXED_CONTENT, false, false),
    new HTMLElement("H3", alignAttr, textModel, MIXED_CONTENT, false, false),
    new HTMLElement("H4", alignAttr, textModel, MIXED_CONTENT, false, false),
    new HTMLElement("H5", alignAttr, textModel, MIXED_CONTENT, false, false),
    new HTMLElement("H6", alignAttr, textModel, MIXED_CONTENT, false, false),
    new HTMLElement("HEAD", noAttr, headModel, ELEMENT_CONTENT, true, true),
    new HTMLElement("HR", hrAttr, noModel, ELEMENT_CONTENT, false, true),
    new HTMLElement("HTML", htmlAttr, htmlModel, ELEMENT_CONTENT, true, true),
    new HTMLElement("I", noAttr, textModel, MIXED_CONTENT, false, false),
    new HTMLElement("IMG", imgAttr, noModel, ELEMENT_CONTENT, false, true),
    new HTMLElement("INPUT", inputAttr, noModel, ELEMENT_CONTENT, false, true),
    new HTMLElement("ISINDEX", isindexAttr, noModel, ELEMENT_CONTENT, false,
                    true),
    new HTMLElement("KBD", noAttr, noModel, MIXED_CONTENT, false, false),
    new HTMLElement("LI", liAttr, flowModel, MIXED_CONTENT, false, true),
    new HTMLElement("LINK", linkAttr, noModel, ELEMENT_CONTENT, false, true),
    new HTMLElement("MAP", mapAttr, mapModel, ELEMENT_CONTENT, false, false),
    new HTMLElement("MENU", compactAttr, liModel, ELEMENT_CONTENT, false,
                    false),
    new HTMLElement("META", metaAttr, noModel, ELEMENT_CONTENT, false, true),
    new HTMLElement("OL", olAttr, liModel, ELEMENT_CONTENT, false, false),
    new HTMLElement("OPTION", optionAttr, noModel, MIXED_CONTENT, false, true),
    new HTMLElement("P", alignAttr, textModel, MIXED_CONTENT, false, true),
    new HTMLElement("PARAM", paramAttr, noModel, ELEMENT_CONTENT, false, true),
    new HTMLElement("PRE", preAttr, textModel, LITERAL_CONTENT, false, false),
    new HTMLElement("SAMP", noAttr, textModel, MIXED_CONTENT, false, false),
    new HTMLElement("SCRIPT", noAttr, noModel, MIXED_CONTENT, false, false),
    new HTMLElement("SELECT", selectAttr, selectModel, ELEMENT_CONTENT, false,
                    false),
    new HTMLElement("SMALL", noAttr, textModel, MIXED_CONTENT, false, false),
    new HTMLElement("STRIKE", noAttr, textModel, MIXED_CONTENT, false, false),
    new HTMLElement("STRONG", noAttr, textModel, MIXED_CONTENT, false, false),
    new HTMLElement("STYLE", noAttr, noModel, ELEMENT_CONTENT, false, false),
    new HTMLElement("SUB", noAttr, textModel, MIXED_CONTENT, false, false),
    new HTMLElement("SUP", noAttr, textModel, MIXED_CONTENT, false, false),
    new HTMLElement("TABLE", tableAttr, tableModel, ELEMENT_CONTENT, false,
                    false),
    new HTMLElement("TD", thtdAttr, bodyModel, MIXED_CONTENT, false, true),
    new HTMLElement("TEXTAREA", textareaAttr, noModel, MIXED_CONTENT, false,
                    false),
    new HTMLElement("TH", thtdAttr, bodyModel, MIXED_CONTENT, false, true),
    new HTMLElement("TITLE", noAttr, noModel, MIXED_CONTENT, false, false),
    new HTMLElement("TR", trAttr, trModel, ELEMENT_CONTENT, false, true),
    new HTMLElement("TT", noAttr, textModel, MIXED_CONTENT, false, false),
    new HTMLElement("U", noAttr, textModel, MIXED_CONTENT, false, false),
    new HTMLElement("UL", ulAttr, liModel, ELEMENT_CONTENT, false, false),
    new HTMLElement("VAR", noAttr, textModel, MIXED_CONTENT, false, false),
    // Used as start element
    new HTMLElement("DOCUMENT", noAttr, documentModel, ELEMENT_CONTENT, true,
                    true)
  };

  private HTMLTokenizer lex;
  private OutputStream out;
  private boolean tracing;

  public HTMLParser(PushbackInputStream in, OutputStream output)
  {
    lex = new HTMLTokenizer(in);
    out = output;
    tracing = false;
  }

  public void setTrace(boolean trace)
  {
    tracing = trace;
  }

  public void parse() throws IOException
  {
    parseElement(HTML_DOCUMENT);
  }

  private int parseElement(int currElement) throws IOException
  {
    return parseElement(currElement, lex.nextToken());
  }

  private int parseElement(int currElement, int token) throws IOException
  {
    while (token != TT_EOF) {
      switch (token) {
      case TT_TEXT:
        switch (elements[currElement].content) {
        case MIXED_CONTENT:
          StringBuffer newText = new StringBuffer();
          StringBuffer text = lex.text;
          int length = text.length();
          boolean hasSpace = false;
          for (int i = 0; i < length; i++) {
            char c = text.charAt(i);
            if (Character.isSpace(c)) {
              if (!hasSpace) {
                newText.append(' ');
                hasSpace = true;
              }
            }
            else {
              newText.append(c);
              hasSpace = false;
            }
          }
          outputText(newText);
          token = lex.nextToken();
          break;

        case ELEMENT_CONTENT:
          if (lex.text.toString().trim().length() == 0) {
            token = lex.nextToken();
          }
          else {
            int nextElement = findNonElementContentElement(currElement);
            if (nextElement != -1) {
              if (tracing) {
                System.out.println("Assumed <" + elements[nextElement].name + 
                                 ">");
              }
              outputStartTag(nextElement);
              token = parseElement(nextElement, TT_TEXT);
              outputEndTag(nextElement);
              if (token == TT_NONE) {
                token = lex.nextToken();
              }
            }
            else if (elements[currElement].endTagOptional) {
              if (tracing) {
                System.out.println("Assumed </" + elements[currElement].name +
                                   ">");
              }
              return TT_TEXT;
            }
            else {
/*
              System.out.println("Text not allowed in " + 
                                 elements[currElement].name);         
*/
              token = lex.nextToken();
            }           
          }
          break;

        case LITERAL_CONTENT:
          outputText(lex.text);
          token = lex.nextToken();
          break;
        }

        break;

      case TT_START_TAG:
        int newElement = findElement(lex.tag.name);
        if (newElement == -1) {
//          System.out.println("Unknown tag <" + lex.tag.name + ">");
          token = lex.nextToken();
        }
        else if (isElementAllowed(currElement, newElement)) {
          outputStartTag(newElement, lex.tag);
          token = parseElement(newElement);
          outputEndTag(newElement);
          if (token == TT_NONE) {
            token = lex.nextToken();
          }
        }
        else {
          int nextElement = findAllowableElement(currElement, newElement);
          if (nextElement != -1) {
            if (tracing) {
              System.out.println("Assumed <" + elements[nextElement].name + 
                                 ">");
            }
            outputStartTag(nextElement);
            token = parseElement(nextElement, TT_START_TAG);
            outputEndTag(nextElement);
            if (token == TT_NONE) {
              token = lex.nextToken();
            }
          }
          else if (elements[currElement].endTagOptional) {
            if (tracing) {
              System.out.println("Assumed </" + elements[currElement].name +
                                 ">");
            }
            return TT_START_TAG;
          }
          else {
/*
            System.out.println("<" + lex.tag.name + "> not allowed in <" +
                               elements[currElement].name + ">");
*/
            token = lex.nextToken();
          }
        }

        break;
        
      case TT_END_TAG:
        int endElement = findElement(lex.tag.name);
        if (endElement == -1) {
//          System.out.println("Unknown tag </" + lex.tag.name + ">");
          token = lex.nextToken();        
        }
        else if (endElement == currElement) {
          return TT_NONE;
        }
        else if (elements[currElement].endTagOptional) {
          if (tracing) {
            System.out.println("Assumed </" + elements[currElement].name +
                               ">");
          }
          return TT_END_TAG;
        }
        else {
/*
          System.out.println("</" + lex.tag.name + "> not allowed in <" +
                             elements[currElement].name + ">");
*/
          token = lex.nextToken();
        }
        break;
      }
    }
    
    lex.tag = new HTMLTag();
    lex.tag.start = false;
    lex.tag.name = new StringBuffer(elements[currElement].name);
    return TT_END_TAG;
  }

  private int findElement(StringBuffer tag)
  {
    int length = elements.length - 1;
    for (int i = 0; i < length; i++) {
      if (tag.toString().equalsIgnoreCase(elements[i].name)) {
        return i;
      }
    }
    return -1;
  }

  private int findNonElementContentElement(int currElement)
  {
    int[] modelElements = elements[currElement].model;
    int numElements = modelElements.length;
    for (int i = 0; i < numElements; i++) {
      int tmpElement = modelElements[i];
      if (elements[tmpElement].startTagOptional) {
        if (elements[tmpElement].content != ELEMENT_CONTENT) {
          return tmpElement;
        }
        else {
          if (findNonElementContentElement(tmpElement) != -1) {
            return tmpElement;
          }
        }
      }
    }
    return -1;
  }

  private boolean isElementAllowed(int currElement, int newElement)
  {
    int[] modelElements = elements[currElement].model;
    int numElements = modelElements.length;    
    for (int i = 0; i < numElements; i++) {
      if (modelElements[i] == newElement) {
        return true;
      }
    }
    return false;
  }

  private int findAllowableElement(int currElement, int newElement)
  {
    int[] modelElements = elements[currElement].model;
    int numElements = modelElements.length;
    for (int i = 0; i < numElements; i++) {
      int tmpElement = modelElements[i];
      if (elements[tmpElement].startTagOptional) {
        if (isElementAllowed(tmpElement, newElement) ||
            findAllowableElement(tmpElement, newElement) != -1) {
          return tmpElement;
        }
      }
    }
    return -1;
  }

  private void outputStartTag(int element) throws IOException
  {
    outputStartTag(element, null);
  }

  private void outputStartTag(int element, HTMLTag tag) throws IOException
  {
    switch (element) {
    case HTML_EM: case HTML_VAR: case HTML_CITE:
      element = HTML_I;
      break;
    case HTML_STRONG: case HTML_DFN:
      element = HTML_B;
      break;
    case HTML_CODE: case HTML_SAMP: case HTML_KBD:
      element = HTML_TT;
      break;
    }

    byte[] start = new byte[2];
    start[0] = 0;
    start[1] = (byte)(element + 1);
    out.write(start);

    if (tag != null) {
      Enumeration attributes = tag.attributes.elements();
      Enumeration values = tag.values.elements();
      while (attributes.hasMoreElements()) {
        StringBuffer attribute = (StringBuffer)(attributes.nextElement());
        StringBuffer value = (StringBuffer)(values.nextElement());
        int attr = findAttribute(element, attribute);
        if (attr == -1) {
/*
          System.out.println("Unknown attribute \"" + attribute + "\" in <" +
                             elements[element].name + ">");
*/
        }
        else {
          byte[] val = new byte[2 + value.length()];
          val[0] = (byte)(attr + 1);
          value.toString().getBytes(0, value.length(), val, 1);
          val[val.length - 1] = 0;
          out.write(val);
        }
      }
    } 

    out.write(0);

  }

  private void outputEndTag(int element) throws IOException
  {
    switch (element) {
    case HTML_EM: case HTML_VAR: case HTML_CITE:
      element = HTML_I;
      break;
    case HTML_STRONG: case HTML_DFN:
      element = HTML_B;
      break;
    case HTML_CODE: case HTML_SAMP: case HTML_KBD:
      element = HTML_TT;
      break;
    }

    byte[] output = new byte[3];
    output[0] = 0;
    output[1] = 0;
    output[2] = (byte)(element + 1);
    out.write(output);
  }

  private void outputText(StringBuffer text) throws IOException
  {
    byte[] output = new byte[text.length()];
    text.toString().getBytes(0, text.length(), output, 0);
    out.write(output);
  }

  private int findAttribute(int element, StringBuffer attribute)
  {
    String[] attributes = elements[element].attributes;
    int length = attributes.length;
    for (int i = 0; i < length; i++) {
      if (attributes[i].equalsIgnoreCase(attribute.toString())) {
        return i;
      }
    }
    return -1;
  }

  /*
  private void outputStartTag(int element)
  {
    System.out.println("<" + elements[element].name + ">");
  }

  private void outputStartTag(HTMLTag tag)
  {
    System.out.print("<" + tag.name);
    Enumeration attributes = tag.attributes.elements();
    Enumeration values = tag.values.elements();
    while (attributes.hasMoreElements()) {
      System.out.print(" " + (StringBuffer)(attributes.nextElement()) +
                       "=" + (StringBuffer)(values.nextElement()));
    }
    System.out.println(">");
  }

  private void outputEndTag(int element)
  {
    System.out.println("</" + elements[element].name + ">");
  }

  private void outputText(StringBuffer text)
  {
    System.out.println(text);
  }
  */
}

