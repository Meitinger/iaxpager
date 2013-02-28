IAX Pager
=========


Description
-----------
*IAX Pager* is a Windows service that communicates with an IAX server (most
likely Asterisk), accepts any calls and routes the inbound channel to a local
sound device. In other words, it turns a Windows PC into an intercom.
Additionally, it can also be configured to play a ringtone instead.


Build
-----
The current Makefile was written for Microsoft's Build Tools, however the
source code should compile under msys/mingw as well. Visual Studio users need
to open the VS command line prompt, navigate to the source folder, type

    nmake

and hit enter.


Install
-------
The Makefile will install and register the service's executable. The default
configuration auto-starts *IAX Pager* and listens on port 4569 for any calls.
You can change the name of the service in the Makefile, or use

    nmake install

without modifications, which will install the service as `iax-pager`.


Usage
-----
To change the behavior of the service, you can set additional parameters in
the service's property page at the service manager `services.msc`. Only the
first character of each parameter gets recognized, so it doesn't matter 
whether you for example specify

    -allow 192.168.0.0/16

or just

    -a 192.168.0.0/16

The following parameters are available:

* `-a[llow] <CIDR>`: if set, a host must be within the given subnet in order
                     to establish a connection and place a call

* `-f[orbid] <CIDR>`: if set, access will be denied to any host within the
                      given subnet (takes precedence over `-a[llow]`)

* `-p[ort] <uint16>`: IAX port the service will listen for incoming connections

* `-v[olume] <uint16>`: sets the output volume during calls

* `-c[ard] <uint32>`: id of the Windows waveform audio output device to use

The `-a[llow]` and `-f[orbid]` parameters can occur more than once, which
allows for a combination of non-overlapping subnets.

If the service is required to register with a server, the following parameters
must be specified:

* `-h[ost] <string>`: name of the registrar server

* `-u[ser] <string>`: account name to register

* `-s[ecret] <string>`: account password

If you don't want the service to answer a call but play a ringtone instead:

* `-r[ing] <filename>`: ring tone file name, must be a waveform audio

* `-l[oop]`: play the file repeatedly as long as the caller keeps ringing
