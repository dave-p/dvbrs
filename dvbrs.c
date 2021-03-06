/*
    dvbrs - report EIT Running Status changes.
    Mercilessly ripped off from dvbdate, Copyright (C) Laurence Culhane 2002
    Mercilessly ripped off from dvbtune, Copyright (C) Dave Chapman 2001
    Published under GPL3.
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/time.h>
#include <stdint.h>
#include <linux/dvb/dmx.h>

size_t freesat_huffman_decode
  (char *dst, size_t* dstlen, const uint8_t *src, size_t srclen);
void report(char *buf, int seclen, int sid, int eid, int rs);

char *ProgName;
int timeout = 25;
int running = 1;

void errmsg(char *message, ...)
{
	va_list ap;

	va_start(ap, message);
	fprintf(stderr, "%s: ", ProgName);
	vfprintf(stderr, message, ap);
	va_end(ap);
}


int main(int argc, char **argv)
{
	int n, seclen;
	char buf[4096];
	struct dmx_sct_filter_params sctFilterParams;
	struct pollfd ufd;
	int res;
	unsigned int now[65536], next[65536];
	int count = 0, sid_req = 0;
	int fd, dev = 0, opt;

	void finish();
	
	setbuf(stdout, NULL);
	ProgName = argv[0];

	while ((opt = getopt(argc, argv, "a:s:")) != -1) {
	    switch (opt) {
		case 'a':
		    dev = atoi(optarg);
		    break;
		case 's':
		    sid_req = atoi(optarg);
		    break;
		default: /* '?' */
		    fprintf(stderr, "Usage: %s [-a adapter-no] [-s SID]\n", argv[0]);
		    exit(EXIT_FAILURE);
	    }
	}

	sprintf(buf, "/dev/dvb/adapter%d/demux0", dev);
	if ((fd = open(buf, O_RDONLY | O_NONBLOCK)) < 0) {
		perror("dvbrs DEVICE: ");
		return -1;
	}

	memset(&sctFilterParams, 0, sizeof(sctFilterParams));
	sctFilterParams.pid = 0x12;
	sctFilterParams.timeout = 0;
	sctFilterParams.flags = DMX_IMMEDIATE_START;
	sctFilterParams.filter.filter[0] = 0x4e;
	sctFilterParams.filter.mask[0] = 0xff;

	if (ioctl(fd, DMX_SET_FILTER, &sctFilterParams) < 0) {
		perror("dvbrs - DMX_SET_FILTER:");
		close(fd);
		return -1;
	}

	signal(SIGINT, finish);

	while (running) {
		memset(&ufd,0,sizeof(ufd));
		ufd.fd=fd;
		ufd.events=POLLIN;

		res = poll(&ufd,1,10000);
		if (res != 1) {
			if (running) errmsg("timeout polling for data\n");
			goto windup;
		}

		if ((n = read(fd, buf, sizeof(buf))) >= 3) {
			seclen = ((buf[1] & 0x0f) << 8) | (buf[2] & 0xff);
			if (n == seclen + 3) {
			    if (n > 0x30) {
				int eid = (buf[14] << 8) + buf[15];
				int rs = (buf[24] & 0xe0) >> 5;
				int sid = (buf[3] << 8) + buf[4];
				if (rs == 1) {
				    if ((!sid_req && (next[sid] != eid)) || (sid_req == sid)) {
					next[sid] = eid;
					report(buf, seclen, sid, eid, rs);
				    }
				}
				else if (rs == 4) {
				    if ((!sid_req && (now[sid] != eid)) || (sid_req == sid)) {
					now[sid] = eid;
					report(buf, seclen, sid, eid, rs);
				    }
				}
			    }
			} else {
				errmsg("Under-read bytes - wanted %d, got %d\n", seclen, n);
				goto windup;
			}
		} else {
			errmsg("Nothing to read - try tuning to a multiplex?\n");
			goto windup;
		}
		count++;
	}
windup:
	close(fd);
	printf("Received %d packets\n", count);
	return 0;
}

void finish(int sig) {
    running = 0;
    return;
}

void report(char *buf, int seclen, int sid, int eid, int rs) {

    struct timespec ts;
    struct tm *tm;
    char tstring[128];
    long ms;
    int p = 0x1a;
    char title[255];
    size_t dstlen = 255;

    while (p < seclen && buf[p] != 0x4d) {
        p += buf[p+1] + 2;
    }
    if (p < seclen) {
        if (buf[p+6] == 0x1f) {
            freesat_huffman_decode(title, &dstlen, &buf[p+6], buf[p+5]);
        }
        else {
            strncpy(title, &buf[p+6], buf[p+5]);
            title[buf[p+5]] = 0;
        }
    else strcpy(title, "<Missing event descriptor>");
    clock_gettime(CLOCK_REALTIME, &ts);
    tm = localtime(&ts.tv_sec);
    ms = ts.tv_nsec / 1000000;
    strftime(tstring, sizeof(tstring), "%T", tm);
    printf("%s:%03d sid %d eid %d, '%s' rs %d\n", tstring, ms, sid, eid, title, rs);
    return;
}
