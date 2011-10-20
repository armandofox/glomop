function FindProxyForURL(url, host)
{
     if (url.substring(0, 5) == "http:") {
         return "PROXY transend.cs.berkeley.edu:4444";
     } else {
         return "DIRECT";
     }
}

