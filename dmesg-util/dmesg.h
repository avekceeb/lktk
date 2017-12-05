#ifndef DMESG_H
#define DMESG_H
int grep_kernel_messages(int level, struct timeval *after, int verbose);
#endif
