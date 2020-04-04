SRCS = ext2_ls.c ext2_cp.c ext2_mkdir.c ext2_ln.c ext2_rm.c ext2_rm_bonus.c
	PROGS = ext2_ls ext2_cp ext2_mkdir ext2_ln ext2_rm ext2_rm_bonus

all : $(PROGS)

$(PROGS) : % : %.c
	gcc -Wall -Werror -g -o $@ $<

clean :
	rm -f $(PROGS)
