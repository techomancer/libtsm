#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stropts.h>

void print_flag(unsigned int value, unsigned int flag, const char *name) {
    if (value & flag) {
        printf(" %s", name);
    }
}

int main() {
    struct termios t;
    if (tcgetattr(STDIN_FILENO, &t) < 0) {
        perror("tcgetattr");
        return 1;
    }

    printf("Termios attributes for STDIN:\n");

    printf("c_iflag (0%o):", (unsigned int)t.c_iflag);
    print_flag(t.c_iflag, IGNBRK, "IGNBRK");
    print_flag(t.c_iflag, BRKINT, "BRKINT");
    print_flag(t.c_iflag, IGNPAR, "IGNPAR");
    print_flag(t.c_iflag, PARMRK, "PARMRK");
    print_flag(t.c_iflag, INPCK, "INPCK");
    print_flag(t.c_iflag, ISTRIP, "ISTRIP");
    print_flag(t.c_iflag, INLCR, "INLCR");
    print_flag(t.c_iflag, IGNCR, "IGNCR");
    print_flag(t.c_iflag, ICRNL, "ICRNL");
#ifdef IUCLC
    print_flag(t.c_iflag, IUCLC, "IUCLC");
#endif
    print_flag(t.c_iflag, IXON, "IXON");
    print_flag(t.c_iflag, IXANY, "IXANY");
    print_flag(t.c_iflag, IXOFF, "IXOFF");
#ifdef IMAXBEL
    print_flag(t.c_iflag, IMAXBEL, "IMAXBEL");
#endif
#ifdef IUTF8
    print_flag(t.c_iflag, IUTF8, "IUTF8");
#endif
    printf("\n");

    printf("c_oflag (0%o):", (unsigned int)t.c_oflag);
    print_flag(t.c_oflag, OPOST, "OPOST");
#ifdef OLCUC
    print_flag(t.c_oflag, OLCUC, "OLCUC");
#endif
    print_flag(t.c_oflag, ONLCR, "ONLCR");
#ifdef OCRNL
    print_flag(t.c_oflag, OCRNL, "OCRNL");
#endif
#ifdef ONOCR
    print_flag(t.c_oflag, ONOCR, "ONOCR");
#endif
#ifdef ONLRET
    print_flag(t.c_oflag, ONLRET, "ONLRET");
#endif
#ifdef OFILL
    print_flag(t.c_oflag, OFILL, "OFILL");
#endif
#ifdef OFDEL
    print_flag(t.c_oflag, OFDEL, "OFDEL");
#endif
    printf("\n");

    printf("c_cflag (0%o):", (unsigned int)t.c_cflag);
    print_flag(t.c_cflag, CSTOPB, "CSTOPB");
    print_flag(t.c_cflag, CREAD, "CREAD");
    print_flag(t.c_cflag, PARENB, "PARENB");
    print_flag(t.c_cflag, PARODD, "PARODD");
    print_flag(t.c_cflag, HUPCL, "HUPCL");
    print_flag(t.c_cflag, CLOCAL, "CLOCAL");
    printf("\n");

    printf("c_lflag (0%o):", (unsigned int)t.c_lflag);
    print_flag(t.c_lflag, ISIG, "ISIG");
    print_flag(t.c_lflag, ICANON, "ICANON");
    print_flag(t.c_lflag, ECHO, "ECHO");
    print_flag(t.c_lflag, ECHOE, "ECHOE");
    print_flag(t.c_lflag, ECHOK, "ECHOK");
    print_flag(t.c_lflag, ECHONL, "ECHONL");
    print_flag(t.c_lflag, NOFLSH, "NOFLSH");
    print_flag(t.c_lflag, TOSTOP, "TOSTOP");
    print_flag(t.c_lflag, IEXTEN, "IEXTEN");
#ifdef ECHOCTL
    print_flag(t.c_lflag, ECHOCTL, "ECHOCTL");
#endif
#ifdef ECHOKE
    print_flag(t.c_lflag, ECHOKE, "ECHOKE");
#endif
    printf("\n");

    printf("c_cc:\n");
    printf("  VINTR:  0x%02x\n", t.c_cc[VINTR]);
    printf("  VQUIT:  0x%02x\n", t.c_cc[VQUIT]);
    printf("  VERASE: 0x%02x\n", t.c_cc[VERASE]);
    printf("  VKILL:  0x%02x\n", t.c_cc[VKILL]);
    printf("  VEOF:   0x%02x\n", t.c_cc[VEOF]);
    printf("  VTIME:  0x%02x\n", t.c_cc[VTIME]);
    printf("  VMIN:   0x%02x\n", t.c_cc[VMIN]);
    printf("  VSTART: 0x%02x\n", t.c_cc[VSTART]);
    printf("  VSTOP:  0x%02x\n", t.c_cc[VSTOP]);
    printf("  VSUSP:  0x%02x\n", t.c_cc[VSUSP]);

    /* Check STREAMS modules */
    struct str_list list;
    struct str_mlist names[10];
    int i;

    list.sl_nmods = 10;
    list.sl_modlist = names;

    if (ioctl(STDIN_FILENO, I_LIST, &list) >= 0) {
        printf("STREAMS modules:");
        for (i = 0; i < list.sl_nmods; i++) {
            printf(" %s", names[i].l_name);
        }
        printf("\n");
    }

    return 0;
}
