#include "ft_ping.h"

uint16_t icmp_checksum(const void *data, size_t len)
{
    const uint16_t *words = (const uint16_t *)data;
    uint32_t sum = 0;

    while (len > 1)
    {
        sum += *words++;
        len -= 2;
    }

    /* Odd byte is padded as (byte, 0x00): memcpy keeps that host-order safe. */
    if (len == 1)
    {
        uint16_t last = 0;
        memcpy(&last, words, 1);
        sum += last;
    }

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return (uint16_t)~sum;
}
