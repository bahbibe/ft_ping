NAME = ft_ping
CC = cc
CFLAGS = -Wall -Werror -Wextra -g3 -D_GNU_SOURCE -Iinclude -MMD
LDFLAGS = -lm

SRC = src/main.c \
      src/checksum.c \
      src/resolve.c \
      src/packet.c \
      src/loop.c \
      src/stats.c

OBJ = $(SRC:.c=.o)

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Run 'sudo setcap cap_net_raw+ep $(NAME)' to allow raw socket usage."

-include $(OBJ:.o=.d)

clean:
	rm -f $(OBJ) $(OBJ:.o=.d)

fclean: clean
	rm -f $(NAME)

re: fclean all

test-docker: all
	docker build -t ft_ping-test -f Dockerfile.test .
	docker run --rm --cap-add=NET_RAW ft_ping-test

.PHONY: all clean fclean re test-docker
