# dvbrs

This utility displays Running Status (Now &amp; Next) info from a DVB stream. It is useful eg. for checking
'accurate recording' performance of digital recorders.

Tested on UK Freeview and Freesat. The Huffman decoder is needed to display programme titles on Freesat;
it is not needed for Freeview.

The tuner must be tuned to the desired multiplex by some other means before starting the program.

### Usage
`dvbrs [-a num] [-s sid]`

By default dvbrs uses the first dvb device `/dev/dvb/adapter0/demux0`, and outputs only changes in running status.

`-a num` Use an alternative dvb adapter number.

`-s sid` Only output running status for Service ID sid. All data received is output, not just changes.
