#include "anarcast.h"

int
main (int argc, char **argv)
{
    struct stat st;
    struct sockaddr_in a;
    unsigned char *key;
    unsigned int l;
    int s, r, k;
    off_t o;
    
    if (argc != 3) {
	fprintf(stderr, "Usage: %s <file> <keyfile>\n", argv[0]);
	exit(2);
    }

    if (stat(argv[1], &st) == -1)
	die("stat() failed");
    
    if ((r = open(argv[1], O_RDONLY)) == -1)
	die("open() failed");

    if ((k = open(argv[2], O_WRONLY|O_CREAT|O_TRUNC, 0644)) == -1)
	die("open() failed");
    
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(PROXY_SERVER_PORT);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	die("socket() failed");

    if (connect(s, &a, sizeof(a)) == -1)
	die("connect() to server failed");
    
    if (writeall(s, "i", 1) != 1 || writeall(s, &st.st_size, 4) != 4)
	die("writeall() of header to server failed");
    
    o = 0;
    if (sendfile(s, r, &o, st.st_size) != st.st_size)
	die("sendfile() failed");
    
    if (readall(s, &l, 4) != 4)
	die("readall() of key length failed");

    if (l > 20000)
	die("bogus key length");
    
    if (!(key = malloc(l)))
	die("malloc() of key buffer failed");
    
    if (readall(s, key, l) != l)
	die("readall() of key failed");
    
    if (writeall(k, key, l) != l)
	die("writeall() of key failed");
    
    return 0;
}

