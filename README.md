# GloMop

From 1994-1999 at UC Berkeley, Armando Fox, Steven Gribble, Eric Brewer, and others worked on the BARWAN (Bay Area Research
Wireless Access Network)/Daedalus project, exploring how to bring Internet connectivity to wireless and mobile devices.

Part of that work involved a transcoding Web proxy that performed image compression/resizing, layout transformation,
and other aggregation and transformation tasks to allow less-than-desktop-PC devices to effectively browse the Internet.

This repo contains the source code, internal design docs, etc.for the GloMop (Global Mobile Computing by Proxy) subproject, including:

* Pythia, the early prototype (`pythia` subdir) that was also presented at WWW-5 in 1996 (`www5paper` subdir);
* TranSend (`tcsproxy` subdir), a cluster-based scalable server that performed various transformations, aggregations, caching, and per-user customization
(TACC) on Web content to serve it to an array of mobile devices - this was reported on at SOSP-16 in 1997;
* Various internal design documents (`doc` and `doc-ps-pdf` subdirs) relating to the project - unfortunately some of these are in FrameMaker
(.fm or .mif) format, which may be hard to find a reader for these days;
* The perl backend (plug-in distiller for tcsproxy) for Top Gun Wingman, the world's first graphical web browser that ran 
on a mobile device (the Palm Pilot), though I've been unable to find the Pilot source code for the client piece.

If you're viewing this, most likely you're an attorney involved in overturning a stupid patent.  In that case, be aware
that all project documents I was able to find are in this public repo; that if I find any others, I'll place them here;
and that we can if necessary make a consulting arrangement if you need specific questions answered about the documents
you find here.

