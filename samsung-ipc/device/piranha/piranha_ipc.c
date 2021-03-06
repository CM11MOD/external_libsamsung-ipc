/*
 * This file is part of libsamsung-ipc.
 *
 * Copyright (C) 2013 Paul Kocialkowski <contact@paulk.fr>
 *
 * libsamsung-ipc is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * libsamsung-ipc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libsamsung-ipc.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <samsung-ipc.h>
#include <ipc.h>

#include "xmm6260.h"
#include "xmm6260_mipi.h"
#include "xmm6260_sec_modem.h"
#include "piranha_ipc.h"

int piranha_ipc_bootstrap(struct ipc_client *client)
{
    void *modem_image_data = NULL;
    int modem_image_fd = -1;
    int modem_boot_fd = -1;

    unsigned char *p;
    int rc;

    if (client == NULL)
        return -1;

    ipc_client_log(client, "Starting piranha modem bootstrap");

    modem_image_fd = open(PIRANHA_MODEM_IMAGE_DEVICE, O_RDONLY);
    if (modem_image_fd < 0) {
        ipc_client_log(client, "Opening modem image device failed");
        goto error;
    }
    ipc_client_log(client, "Opened modem image device");

    modem_image_data = mmap(0, PIRANHA_MODEM_IMAGE_SIZE, PROT_READ, MAP_SHARED, modem_image_fd, 0);
    if (modem_image_data == NULL || modem_image_data == (void *) 0xffffffff) {
            ipc_client_log(client, "Mapping modem image data to memory failed");
            goto error;
    }
    ipc_client_log(client, "Mapped modem image data to memory");

    modem_boot_fd = open(XMM6260_SEC_MODEM_BOOT0_DEVICE, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (modem_boot_fd < 0) {
        ipc_client_log(client, "Opening modem boot device failed");
        goto error;
    }
    ipc_client_log(client, "Opened modem boot device");

    rc = xmm6260_sec_modem_power(modem_boot_fd, 0);
    if (rc < 0) {
        ipc_client_log(client, "Turning the modem off failed");
        goto error;
    }
    ipc_client_log(client, "Turned the modem off");

    rc = xmm6260_sec_modem_power(modem_boot_fd, 1);
    if (rc < 0) {
        ipc_client_log(client, "Turning the modem on failed");
        goto error;
    }
    ipc_client_log(client, "Turned the modem on");

    p = (unsigned char *) modem_image_data + PIRANHA_PSI_OFFSET;

    rc = xmm6260_mipi_psi_send(client, modem_boot_fd, (void *) p, PIRANHA_PSI_SIZE);
    if (rc < 0) {
        ipc_client_log(client, "Sending XMM6260 MIPI PSI failed");
        goto error;
    }
    ipc_client_log(client, "Sent XMM6260 MIPI PSI");

    close(modem_boot_fd);

    modem_boot_fd = open(XMM6260_SEC_MODEM_BOOT1_DEVICE, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (modem_boot_fd < 0) {
        ipc_client_log(client, "Opening modem boot device failed");
        goto error;
    }
    ipc_client_log(client, "Opened modem boot device");

    p = (unsigned char *) modem_image_data + PIRANHA_EBL_OFFSET;

    rc = xmm6260_mipi_ebl_send(client, modem_boot_fd, (void *) p, PIRANHA_EBL_SIZE);
    if (rc < 0) {
        ipc_client_log(client, "Sending XMM6260 MIPI EBL failed");
        goto error;
    }
    ipc_client_log(client, "Sent XMM6260 MIPI EBL");

    rc = xmm6260_mipi_port_config_send(client, modem_boot_fd);
    if (rc < 0) {
        ipc_client_log(client, "Sending XMM6260 MIPI port config failed");
        goto error;
    }
    ipc_client_log(client, "Sent XMM6260 MIPI port config");

    p = (unsigned char *) modem_image_data + PIRANHA_SEC_START_OFFSET;

    rc = xmm6260_mipi_sec_start_send(client, modem_boot_fd, (void *) p, PIRANHA_SEC_START_SIZE);
    if (rc < 0) {
        ipc_client_log(client, "Sending XMM6260 MIPI SEC start failed");
        goto error;
    }
    ipc_client_log(client, "Sent XMM6260 MIPI SEC start");

    p = (unsigned char *) modem_image_data + PIRANHA_FIRMWARE_OFFSET;

    rc = xmm6260_mipi_firmware_send(client, modem_boot_fd, (void *) p, PIRANHA_FIRMWARE_SIZE);
    if (rc < 0) {
        ipc_client_log(client, "Sending XMM6260 MIPI firmware failed");
        goto error;
    }
    ipc_client_log(client, "Sent XMM6260 MIPI firmware");

    rc = xmm6260_mipi_nv_data_send(client, modem_boot_fd);
    if (rc < 0) {
        ipc_client_log(client, "Sending XMM6260 MIPI nv_data failed");
        goto error;
    }
    ipc_client_log(client, "Sent XMM6260 MIPI nv_data");

    rc = xmm6260_mipi_sec_end_send(client, modem_boot_fd);
    if (rc < 0) {
        ipc_client_log(client, "Sending XMM6260 MIPI SEC end failed");
        goto error;
    }
    ipc_client_log(client, "Sent XMM6260 MIPI SEC end");

    rc = xmm6260_mipi_hw_reset_send(client, modem_boot_fd);
    if (rc < 0) {
        ipc_client_log(client, "Sending XMM6260 MIPI HW reset failed");
        goto error;
    }
    ipc_client_log(client, "Sent XMM6260 MIPI HW reset");

    rc = 0;
    goto complete;

error:
    rc = -1;

complete:
    if (modem_image_data != NULL)
        munmap(modem_image_data, PIRANHA_MODEM_IMAGE_SIZE);

    if (modem_image_fd >= 0)
        close(modem_image_fd);

    if (modem_boot_fd >= 0)
        close(modem_boot_fd);

    return rc;
}

int piranha_ipc_fmt_send(struct ipc_client *client, struct ipc_message_info *request)
{
    return xmm6260_sec_modem_ipc_fmt_send(client, request);
}

int piranha_ipc_fmt_recv(struct ipc_client *client, struct ipc_message_info *response)
{
    return xmm6260_sec_modem_ipc_fmt_recv(client, response);
}

int piranha_ipc_rfs_send(struct ipc_client *client, struct ipc_message_info *request)
{
    return xmm6260_sec_modem_ipc_rfs_send(client, request);
}

int piranha_ipc_rfs_recv(struct ipc_client *client, struct ipc_message_info *response)
{
    return xmm6260_sec_modem_ipc_rfs_recv(client, response);
}

int piranha_ipc_open(void *data, int type)
{
    struct piranha_ipc_transport_data *transport_data;

    if (data == NULL)
        return -1;

    transport_data = (struct piranha_ipc_transport_data *) data;

    transport_data->fd = xmm6260_sec_modem_ipc_open(type);
    if (transport_data->fd < 0)
        return -1;

    return 0;
}

int piranha_ipc_close(void *data)
{
    struct piranha_ipc_transport_data *transport_data;

    if (data == NULL)
        return -1;

    transport_data = (struct piranha_ipc_transport_data *) data;

    xmm6260_sec_modem_ipc_close(transport_data->fd);
    transport_data->fd = -1;

    return 0;
}

int piranha_ipc_read(void *data, void *buffer, unsigned int length)
{
    struct piranha_ipc_transport_data *transport_data;
    int rc;

    if (data == NULL)
        return -1;

    transport_data = (struct piranha_ipc_transport_data *) data;

    rc = xmm6260_sec_modem_ipc_read(transport_data->fd, buffer, length);
    return rc;
}

int piranha_ipc_write(void *data, void *buffer, unsigned int length)
{
    struct piranha_ipc_transport_data *transport_data;
    int rc;

    if (data == NULL)
        return -1;

    transport_data = (struct piranha_ipc_transport_data *) data;

    rc = xmm6260_sec_modem_ipc_write(transport_data->fd, buffer, length);
    return rc;
}

int piranha_ipc_poll(void *data, struct timeval *timeout)
{
    struct piranha_ipc_transport_data *transport_data;
    int rc;

    if (data == NULL)
        return -1;

    transport_data = (struct piranha_ipc_transport_data *) data;

    rc = xmm6260_sec_modem_ipc_poll(transport_data->fd, timeout);
    return rc;
}

int piranha_ipc_power_on(void *data)
{
    return 0;
}

int piranha_ipc_power_off(void *data)
{
    int fd;
    int rc;

    fd = open(XMM6260_SEC_MODEM_BOOT0_DEVICE, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0)
        return -1;

    rc = xmm6260_sec_modem_power(fd, 0);

    close(fd);

    if (rc < 0)
        return -1;

    return 0;
}

int piranha_ipc_data_create(void **transport_data, void **power_data, void **gprs_data)
{
    if (transport_data == NULL)
        return -1;

    *transport_data = (void *) malloc(sizeof(struct piranha_ipc_transport_data));
    memset(*transport_data, 0, sizeof(struct piranha_ipc_transport_data));

    return 0;
}

int piranha_ipc_data_destroy(void *transport_data, void *power_data, void *gprs_data)
{
    if (transport_data == NULL)
        return -1;

    free(transport_data);

    return 0;
}

char *piranha_ipc_gprs_get_iface(int cid)
{
    return xmm6260_sec_modem_ipc_gprs_get_iface(cid);
}


int piranha_ipc_gprs_get_capabilities(struct ipc_client_gprs_capabilities *capabilities)
{
    return xmm6260_sec_modem_ipc_gprs_get_capabilities(capabilities);
}

struct ipc_ops piranha_ipc_fmt_ops = {
    .bootstrap = piranha_ipc_bootstrap,
    .send = piranha_ipc_fmt_send,
    .recv = piranha_ipc_fmt_recv,
};

struct ipc_ops piranha_ipc_rfs_ops = {
    .bootstrap = NULL,
    .send = piranha_ipc_rfs_send,
    .recv = piranha_ipc_rfs_recv,
};

struct ipc_handlers piranha_ipc_handlers = {
    .read = piranha_ipc_read,
    .write = piranha_ipc_write,
    .open = piranha_ipc_open,
    .close = piranha_ipc_close,
    .poll = piranha_ipc_poll,
    .transport_data = NULL,
    .power_on = piranha_ipc_power_on,
    .power_off = piranha_ipc_power_off,
    .power_data = NULL,
    .gprs_activate = NULL,
    .gprs_deactivate = NULL,
    .gprs_data = NULL,
    .data_create = piranha_ipc_data_create,
    .data_destroy = piranha_ipc_data_destroy,
};

struct ipc_gprs_specs piranha_ipc_gprs_specs = {
    .gprs_get_iface = piranha_ipc_gprs_get_iface,
    .gprs_get_capabilities = piranha_ipc_gprs_get_capabilities,
};

// vim:ts=4:sw=4:expandtab
