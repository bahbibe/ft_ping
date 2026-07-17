NAME = ft_ping
CC = cc
CFLAGS = -Wall -Werror -Wextra -g3 -Iincludes -D_GNU_SOURCE
LDFLAGS = -lm

SRC_DIR = src
INC_DIR = include

SRC = $(SRC_DIR)/main.c \
      $(SRC_DIR)/checksum.c \
      $(SRC_DIR)/resolve.c \
      $(SRC_DIR)/packet.c \
      $(SRC_DIR)/loop.c

OBJ = $(SRC:.c=.o)
DEP = $(OBJ:.o=.d)

CFLAGS += -I$(INC_DIR) -MMD

all: $(NAME)
	@echo "Compilation finished successfully."
	@echo "Run sudo setcap cap_net_raw+ep $(NAME) to allow raw socket usage."
$(NAME): $(OBJ)
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

-include $(DEP)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c $(INC_DIR)/ft_ping.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(DEP)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re

.SILENT: $(SRC_DIR)/%.o
