import java.io.*;

class HTMLDecoder
{
  protected InputStream in;

  public HTMLDecoder(InputStream input)
  {
    in = input;
  }

  public void decode() throws IOException
  {
    byte b = (byte)in.read();

    while (b != -1) {
      if (b == 0) {
        b = (byte)in.read();
        if (b == 0) {    // End tag.
          b = (byte)in.read();    // End tag ID.
//          System.out.println("End tag: " + (b - 1));
        }
        else {    // Start tag.
//          System.out.print("Start tag: " + (b - 1));
          b = (byte)in.read();
          while (b != 0) {
            // Now b is attribute ID.
//            System.out.print(" " + (b - 1) + "=");
            // Get the value.
            StringBuffer value = new StringBuffer();
            while ((b = (byte)in.read()) != 0) {
              value.append((char)b);
            }
//            System.out.print(value);
            b = (byte)in.read();    // Get next byte.
          }
//          System.out.println();
        }
        b = (byte)in.read();    // Get next byte.
      }

      else {    // Plain text.
        StringBuffer text = new StringBuffer();
        do {
          text.append((char)b);
        } while ((b = (byte)in.read()) != 0 && b != -1);
//          System.out.println(text);
      }
    }
  }
}


          
          
          

      