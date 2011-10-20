import java.io.*;
import java.net.*;

class RequestHandler extends Thread
{
  private Socket client;
  private DataInputStream clientIn;
  private OutputStream clientOut;
  private InputStream serverIn;    
  private final static int bufferSize = 4096;

  public RequestHandler(Socket c) throws IOException
  {
    client = c;
    try {
      clientIn = new DataInputStream(client.getInputStream());
      clientOut = client.getOutputStream();
    } catch (IOException e) {
      System.out.println(e);
      closeAll();
      throw new IOException();
    }
  }

  public void run()
  {
    String request;
    try {
      request = clientIn.readLine();
    } catch (IOException e) {
      System.out.println(e);
      closeAll();
      return;
    }

    String urlString;
    byte[] response;
    URL url;
    try {
      urlString = request.substring(4, request.lastIndexOf(' '));
      System.out.println(urlString);
      url =  new URL(urlString);
    } catch (StringIndexOutOfBoundsException e) {
      System.out.println("Malformed HTTP request: " + request);
      respondBadRequest();
      return;
    } catch (MalformedURLException e) {
      respondBadRequest();
      return;
    }

    URLConnection urlConnection;
    try {
      urlConnection = url.openConnection();
      serverIn = urlConnection.getInputStream();
    } catch (IOException e) {
      respondNotFound();
      return;
    }

    String contentType = urlConnection.getContentType();
    // Let's forget content length
    // Assume the server will close the connection (no persistent connection)
    // int contentLength = urlConnection.getContentLength();
    if (contentType.equals("text/html")) {
      HTMLParser parser = new HTMLParser(new PushbackInputStream(new BufferedInputStream(serverIn)),
					 clientOut);
      parser.setTrace(true);
      respondOK("application/x-htmlcmp", urlConnection.getURL().toString());
      try {
	parser.parse();
      } catch (IOException e) {
	System.out.println(e);       
      } 
    }
    else if (contentType.equals("text/plain") ||
	     contentType.equals("image/gif")) {
      respondOK(contentType, urlConnection.getURL().toString());
      copyEntityBody();
    }
    else {
      respondNotSupported();
    }

    closeAll();
  }

  private void copyEntityBody()
  {
    byte[] buf = new byte[bufferSize];
    int numRead;
    try {
      while ((numRead = serverIn.read(buf, 0, buf.length)) != -1) {
	clientOut.write(buf, 0, numRead);
      }
    } catch (IOException e) {
      System.out.println(e);
    }
  }

  private void respondOK(String contentType, String location)
  {  
    String okay = "HTTP/1.0 200 OK\r\nContent-Type: ";
    String Location = "Location: ";
    byte[] response = new byte[okay.length() + contentType.length() + 2 +
			      Location.length() + location.length() + 4];
    int i = 0;
    okay.getBytes(0, okay.length(), response, i);
    i += okay.length();
    contentType.getBytes(0, contentType.length(), response, i);
    i += contentType.length();
    response[i] = '\r';
    response[i + 1] = '\n';
    i += 2;
    Location.getBytes(0, Location.length(), response, i);
    i += Location.length();
    location.getBytes(0, location.length(), response, i);
    i += location.length();
    response[i] = '\r';
    response[i + 1] = '\n';
    response[i + 2] = '\r';
    response[i + 3] = '\n';
    sendResponseHeader(response);
  }

  private void respondBadRequest()
  {
    String badRequest = "HTTP/1.0 400 Bad Request\r\n\r\n";
    byte[] response = new byte[badRequest.length()];
    badRequest.getBytes(0, badRequest.length(), response, 0);
    respondAndClose(response);
  }

  private void respondNotFound()
  {
    String notFound = "HTTP/1.0 404 Not Found\r\n\r\n";
    byte[] response = new byte[notFound.length()];
    notFound.getBytes(0, notFound.length(), response, 0);
    respondAndClose(response);
  }

  private void respondNotSupported()
  {
    String notFound = "HTTP/1.0 500 Not Supported\r\n\r\n";
    byte[] response = new byte[notFound.length()];
    notFound.getBytes(0, notFound.length(), response, 0);
    respondAndClose(response);
  }

  private void respondAndClose(byte[] response)
  {
    sendResponseHeader(response);
    closeAll();
  }

  private void sendResponseHeader(byte[] header)
  {
    try {
      clientOut.write(header, 0, header.length);
      clientOut.flush();
    } catch (IOException e) {
      closeAll();
    }
  }

  private void closeAll()
  {
    try {
      if (clientIn != null) {
	clientIn.close();
      }
    } catch (IOException e) {
      System.out.println(e);          
    } finally {
      clientIn = null;
    }

    try {
      if (clientOut != null) {
	clientOut.close();
      }
    } catch (IOException e) {
      System.out.println(e);          
    } finally {
      clientOut = null;
    }

    try {
      if (client != null) {
	client.close();
      }
    } catch (IOException e) {
      System.out.println(e);          
    } finally {
      client = null;
    }

    try {
      if (serverIn != null) {
	serverIn.close();
      }
    } catch (IOException e) {
      System.out.println(e);          
    } finally {
      serverIn = null;
    }
  }
}

class Proxy
{
  public static void main(String[] args)
  {
    if (args.length != 1) {
      System.out.println("Usage: java Proxy port");
      return;
    }

    int port;
    ServerSocket s;
    try {
      port = Integer.parseInt(args[0]);
      s = new ServerSocket(port);
    } catch (NumberFormatException e) {
      System.out.println("port should be an integer");
      return;
    } catch (IOException e) {
      System.out.println(e);
      return;
    }

    while (true) {
      Socket client;
      try {
        client = s.accept();
      } catch (IOException e) {
        System.out.println(e);          
        continue;
      }

      try {
	new RequestHandler(client).start();
      } catch (IOException e) {
        System.out.println(e);          
      }	
    }
  }
}
