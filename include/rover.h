namespace rover {

typedef struct rx_data {
    u8 d1;
    u8 d2;
    u16 data;
} rx_data;

typedef struct tx_data {
    s8 left_motor;
    s8 right_motor;
    u8 param_1;
    u8 param_2;
} tx_data;


int uart_connect();
void tx_uart(int uart_fd, tx_data data);
rx_data rx_uart(int uart_fd);

}

namespace web {
    int rover_fn(std::vector<std::string> params, int socket);
}

