/*####COPYRIGHTBEGIN####
 -------------------------------------------
 Copyright (C) 2006 Steve Karg

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to:
 The Free Software Foundation, Inc.
 59 Temple Place - Suite 330
 Boston, MA  02111-1307, USA.

 As a special exception, if other files instantiate templates or
 use macros or inline functions from this file, or you compile
 this file and link it with other works to produce a work based
 on this file, this file does not by itself cause the resulting
 work to be covered by the GNU General Public License. However
 the source code for this file must still be made available in
 accordance with section (3) of the GNU General Public License.

 This exception does not invalidate any other reasons why a work
 based on this file might be covered by the GNU General Public
 License.
 -------------------------------------------
####COPYRIGHTEND####*/  
    
#include <stdint.h>             /* for standard integer types uint8_t etc. */
#include <stdbool.h>            /* for the standard bool type. */
#include <time.h>            /* for the standard bool type. */
#include "bacdcode.h"
#include "bip.h"
#include "net.h"                /* custom per port */
    
/* Handle the BACnet Virtual Link Control (BVLC), which includes:
   BACnet Broadcast Management Device,
   Broadcast Distribution Table, and
   Foreign Device Registration */ 
typedef struct  {
    
        /* true if valid entry - false if not */ 
    bool valid;
    
        /* BACnet/IP address */ 
    struct in_addr dest_address;
    uint16_t dest_port;
    
        /* Broadcast Distribution Mask - stored in host byte order */ 
    struct in_addr broadcast_mask;
} BBMD_TABLE_ENTRY;

#define MAX_BBMD_ENTRIES 128
static BBMD_TABLE_ENTRY BBMD_Table[MAX_BBMD_ENTRIES];

/*Each device that registers as a foreign device shall be placed
in an entry in the BBMD's Foreign Device Table (FDT). Each
entry shall consist of the 6-octet B/IP address of the registrant;
the 2-octet Time-to-Live value supplied at the time of
registration; and a 2-octet value representing the number of
seconds remaining before the BBMD will purge the registrant's FDT
entry if no re-registration occurs. This value will be initialized
to the 2-octet Time-to-Live value supplied at the time of
registration.*/ 
typedef struct  {
    bool valid;
    
        /* BACnet/IP address */ 
    struct in_addr dest_address;
    uint16_t dest_port;
    
        /* seconds for valid entry lifetime */ 
     uint16_t time_to_live;
    time_t seconds_remaining;  /* includes 30 second grace period */
} FD_TABLE_ENTRY;

#define MAX_FD_ENTRIES 128
static FD_TABLE_ENTRY FD_Table[MAX_FD_ENTRIES];
void bvlc_maintenance_timer(unsigned seconds) 
{
    unsigned i = 0;
    for (i = 0; i < MAX_FD_ENTRIES; i++) {
        if (FD_Table[i].valid)
             {
            if (FD_Table[i].seconds_remaining)
                 {
                if (FD_Table[i].seconds_remaining < seconds)
                    FD_Table[i].seconds_remaining = 0;
                
                else
                    FD_Table[i].seconds_remaining -= seconds;
                if (FD_Table[i].seconds_remaining == 0)
                     {
                    FD_Table[i].valid = false;
                    }
                }
            }
    }
}
int bvlc_encode_bip_address( uint8_t * pdu,  struct in_addr *address, /* in host format */ 
    uint16_t port) 
{
    int len = 0;
    if (pdu) {
        len = encode_unsigned32(&pdu[0], address->s_addr);
        len += encode_unsigned16(&pdu[len], port);
    }
    return len;
}
int bvlc_decode_bip_address( uint8_t * pdu,  struct in_addr *address, /* in host format */ 
    uint16_t * port) 
{
} 

/* used for both read and write entries */ 
int bvlc_encode_address_entry(
    uint8_t * pdu, 
    struct in_addr *address, uint16_t port, struct in_addr *mask) 
{
    int len = 0;
    if (pdu) {
        len = bvlc_encode_bip_address(pdu, address, port);
        len += encode_unsigned32(&pdu[len], mask->s_addr);
    }
    return len;
}
int bvlc_encode_bvlc_result(
    uint8_t * pdu,  BACNET_BVLC_RESULT result_code) 
{
    if (pdu) {
        pdu[0] = BVLL_TYPE_BACNET_IP;
        pdu[1] = BVLC_RESULT;
        
            /* The 2-octet BVLC Length field is the length, in octets,
               of the entire BVLL message, including the two octets of the
               length field itself, most significant octet first. */ 
            encode_unsigned16(&pdu[2], 6);
        encode_unsigned16(&pdu[4], result_code);
    }
    return 6;
}
int bvlc_encode_write_bdt_init( uint8_t * pdu,  unsigned entries) 
{
    int len = 0;
    if (pdu) {
        pdu[0] = BVLL_TYPE_BACNET_IP;
        pdu[1] = BVLC_WRITE_BROADCAST_DISTRIBUTION_TABLE;
        
            /* The 2-octet BVLC Length field is the length, in octets,
               of the entire BVLL message, including the two octets of the
               length field itself, most significant octet first. */ 
            encode_unsigned16(&pdu[2], 4 + entries * 10);
        len = 4;
    }
    return len;
}
int bvlc_encode_read_bdt( uint8_t * pdu) 
{
    int len = 0;
    if (pdu) {
        pdu[0] = BVLL_TYPE_BACNET_IP;
        pdu[1] = BVLC_READ_BROADCAST_DISTRIBUTION_TABLE;
        
            /* The 2-octet BVLC Length field is the length, in octets,
               of the entire BVLL message, including the two octets of the
               length field itself, most significant octet first. */ 
            encode_unsigned16(&pdu[2], 4);
        len = 4;
    }
    return len;
}
int bvlc_encode_read_bdt_ack_init( uint8_t * pdu,  unsigned entries) 
{
    int len = 0;
    if (pdu) {
        pdu[0] = BVLL_TYPE_BACNET_IP;
        pdu[1] = BVLC_READ_BROADCAST_DISTRIBUTION_TABLE_ACK;
        
            /* The 2-octet BVLC Length field is the length, in octets,
               of the entire BVLL message, including the two octets of the
               length field itself, most significant octet first. */ 
            encode_unsigned16(&pdu[2], 4 + entries * 10);
        len = 4;
    }
    return len;
}
int bvlc_encode_forwarded_npdu(
    uint8_t * pdu, 
    BACNET_ADDRESS * src,  uint8_t * npdu,  unsigned npdu_length) 
{
    int len = 0;
    unsigned i;                /* for loop counter */
    if (pdu) {
        pdu[0] = BVLL_TYPE_BACNET_IP;
        pdu[1] = BVLC_FORWARDED_NPDU;
        
            /* The 2-octet BVLC Length field is the length, in octets,
               of the entire BVLL message, including the two octets of the
               length field itself, most significant octet first. */ 
            encode_unsigned16(&pdu[2], 4 + 6 + npdu_length);
        len = 4;
        for (i = 0; i < 6; i++) {
            pdu[len] = src->adr[i];
            len++;
        }
        for (i = 0; i < npdu_length; i++) {
            pdu[len] = npdu[i];
            len++;
        }
    }
    return len;
}
int bvlc_encode_register_foreign_device(
    uint8_t * pdu,  uint16_t time_to_live_seconds) 
{
    int len = 0;
    if (pdu) {
        pdu[0] = BVLL_TYPE_BACNET_IP;
        pdu[1] = BVLC_REGISTER_FOREIGN_DEVICE;
        
            /* The 2-octet BVLC Length field is the length, in octets,
               of the entire BVLL message, including the two octets of the
               length field itself, most significant octet first. */ 
            encode_unsigned16(&pdu[2], 6);
        encode_unsigned16(&pdu[2], time_to_live_seconds);
        len = 6;
    }
    return len;
}
int bvlc_encode_read_fdt( uint8_t * pdu) 
{
    int len = 0;
    if (pdu) {
        pdu[0] = BVLL_TYPE_BACNET_IP;
        pdu[1] = BVLC_READ_FOREIGN_DEVICE_TABLE;
        
            /* The 2-octet BVLC Length field is the length, in octets,
               of the entire BVLL message, including the two octets of the
               length field itself, most significant octet first. */ 
            encode_unsigned16(&pdu[2], 4);
        len = 4;
    }
    return len;
}
int bvlc_encode_read_fdt_ack_init( uint8_t * pdu,  unsigned entries) 
{
    int len = 0;
    if (pdu) {
        pdu[0] = BVLL_TYPE_BACNET_IP;
        pdu[1] = BVLC_READ_FOREIGN_DEVICE_TABLE_ACK;
        
            /* The 2-octet BVLC Length field is the length, in octets,
               of the entire BVLL message, including the two octets of the
               length field itself, most significant octet first. */ 
            encode_unsigned16(&pdu[2], 4 + entries * 10);
        len = 4;
    }
    return len;
}
int bvlc_encode_delete_fdt_entry(
    uint8_t * pdu,  struct in_addr *address, uint16_t port) 
{
    int len = 0;
    if (pdu) {
        pdu[0] = BVLL_TYPE_BACNET_IP;
        pdu[1] = BVLC_READ_FOREIGN_DEVICE_TABLE;
        
            /* The 2-octet BVLC Length field is the length, in octets,
               of the entire BVLL message, including the two octets of the
               length field itself, most significant octet first. */ 
            encode_unsigned16(&pdu[2], 10);
        
            /* FDT Entry */ 
            encode_unsigned32(&pdu[0], address->s_addr);
        encode_unsigned16(&pdu[4], port);
        len = 10;
    }
    return len;
}
int bvlc_encode_distribute_broadcast_to_network(
    uint8_t * pdu,  uint8_t * npdu,  unsigned npdu_length) 
{
    int len = 0;               /* return value */
    unsigned i;                /* for loop counter */
    if (pdu) {
        pdu[0] = BVLL_TYPE_BACNET_IP;
        pdu[1] = BVLC_DISTRIBUTE_BROADCAST_TO_NETWORK;
        
            /* The 2-octet BVLC Length field is the length, in octets,
               of the entire BVLL message, including the two octets of the
               length field itself, most significant octet first. */ 
            len = encode_unsigned16(&pdu[2], 4 + npdu_length) + 2;
        for (i = 0; i < npdu_length; i++) {
            pdu[len] = npdu[i];
            len++;
        }
    }
    return len;
}
int bvlc_encode_original_unicast_npdu(
    uint8_t * pdu,  uint8_t * npdu,  unsigned npdu_length) 
{
    int len = 0;               /* return value */
    unsigned i = 0;            /* loop counter */
    if (pdu) {
        pdu[0] = BVLL_TYPE_BACNET_IP;
        pdu[1] = BVLC_ORIGINAL_UNICAST_NPDU;
        
            /* The 2-octet BVLC Length field is the length, in octets,
               of the entire BVLL message, including the two octets of the
               length field itself, most significant octet first. */ 
            len = encode_unsigned16(&pdu[2], 4 + npdu_length) + 2;
        for (i = 0; i < npdu_length; i++) {
            pdu[len] = npdu[i];
            len++;
        }
    }
    return len;
}
int bvlc_encode_original_broadcast_npdu(
    uint8_t * pdu,  uint8_t * npdu,  unsigned npdu_length) 
{
    int len = 0;               /* return value */
    unsigned i = 0;            /* loop counter */
    if (pdu) {
        pdu[0] = BVLL_TYPE_BACNET_IP;
        pdu[1] = BVLC_ORIGINAL_BROADCAST_NPDU;
        
            /* The 2-octet BVLC Length field is the length, in octets,
               of the entire BVLL message, including the two octets of the
               length field itself, most significant octet first. */ 
            len = encode_unsigned16(&pdu[2], 4 + npdu_length) + 2;
        for (i = 0; i < npdu_length; i++) {
            pdu[len] = npdu[i];
            len++;
        }
    }
    return len;
}


/* copy the source internet address to the BACnet address */ 
/* FIXME: IPv6? */ 
/* FIXME: is sockaddr_in host or network order? */ 
void bvlc_internet_to_bacnet_address( BACNET_ADDRESS * src, /* returns the BACnet source address */ 
    struct sockaddr_in *sin) /* source internet address */ 
{
    int len = 0;
    if (src && sin)
         {
        len = encode_unsigned32(&src->mac[0], sin->sin_addr.s_addr);
        len += encode_unsigned16(&src->mac[4], sin->sin_port);
        src->mac_len = len;
        src->net = 0;
        src->len = 0;
        }
    return;
}


/* copy the source internet address to the BACnet address */ 
/* FIXME: IPv6? */ 
/* FIXME: is sockaddr_in host or network order? */ 
void bvlc_bacnet_to_internet_address( struct sockaddr_in *sin, /* source internet address */ 
    BACNET_ADDRESS * src) /* returns the BACnet source address */ 
{
    int len = 0;
    if (src && sin)
         {
        if (src->mac_len == 6)
             {
            len = decode_unsigned32(&src->mac[0], &sin->sin_addr.s_addr);
            len += decode_unsigned16(&src->mac[4], &sin->sin_port);
            }
        }
    return;
}
void bvlc_bdt_forward_npdu( struct sockaddr_in *sin, /* the source address */ 
    uint8_t * npdu, /* the NPDU */ 
    uint16_t npdu_length) /* length of the NPDU  */ 
{
    uint8_t mtu[MAX_MPDU] = {
    0};
    int mtu_len = 0;
    int bytes_sent = 0;
    unsigned i = 0;            /* loop counter */
    struct sockaddr_in bip_dest;
    
        /* assumes that the driver has already been initialized */ 
        if (bip_socket() < 0)
        return;
    mtu_len =
        bvlc_encode_forwarded_npdu(&mtu[0], sin, npdu, npdu_length);
    
        /* load destination IP address */ 
        bip_dest.sin_family = AF_INET;
    
        /* loop through the BDT and send one to each entry, except us */ 
        for (i = 0; i < MAX_BBMD_ENTRIES; i++)
         {
        if (BBMD_Table[i].valid)
             {
            
                /* The B/IP address to which the Forwarded-NPDU message is
                   sent is formed by inverting the broadcast distribution
                   mask in the BDT entry and logically ORing it with the
                   BBMD address of the same entry. */ 
                bip_dest.sin_addr.s_addr =
                htonl(((~BBMD_Table[i].broadcast_mask.
                        s_addr) | BBMD_Table[i].dest_address.s_addr));
            bip_dest.sin_port = htons(BBMD_Table[i].dest_port);
            
                /* Send the packet */ 
                bytes_sent =
                sendto(bip_socket(), (char *) mtu, mtu_len, 0,
                (struct sockaddr *) bip_dest, sizeof(struct sockaddr));
            }
        return bytes_sent;
        }
    void bvlc_fdt_forward_npdu( struct sockaddr_in *sin, /* the source address */ 
        uint8_t * npdu, /* returns the NPDU */ 
        uint16_t max_npdu) /* amount of space available in the NPDU  */ 
    {
    } uint16_t bvlc_handler(BACNET_ADDRESS * src, /* returns the source address */ 
        uint8_t * npdu, /* returns the NPDU */ 
        uint16_t max_npdu, /* amount of space available in the NPDU  */ 
        unsigned timeout) /* number of milliseconds to wait for a packet */ 
    {
        int received_bytes;
        uint8_t buf[MAX_MPDU] = {
        0};                     /* data */
        uint16_t pdu_len = 0;  /* return value */
        fd_set read_fds;
        int max;
        struct timeval select_timeout;
        struct sockaddr_in sin = { -1 };
        socklen_t sin_len = sizeof(sin);
        int function_type = 0;
        
            /* Make sure the socket is open */ 
            if (BIP_Socket < 0)
            return 0;
        
            /* we could just use a non-blocking socket, but that consumes all
               the CPU time.  We can use a timeout; it is only supported as
               a select. */ 
            if (timeout >= 1000) {
            select_timeout.tv_sec = timeout / 1000;
            select_timeout.tv_usec =
                1000 * (timeout - select_timeout.tv_sec * 1000);
        } else {
            select_timeout.tv_sec = 0;
            select_timeout.tv_usec = 1000 * timeout;
        }
        FD_ZERO(&read_fds);
        FD_SET((unsigned int) BIP_Socket, &read_fds);
        max = BIP_Socket;
        
            /* see if there is a packet for us */ 
            if (select(max + 1, &read_fds, NULL, NULL,
                &select_timeout) > 0)
            received_bytes =
                recvfrom(BIP_Socket, (char *) &buf[0], MAX_MPDU, 0,
                (struct sockaddr *) &sin, &sin_len);
        
        else
            return 0;
        
            /* See if there is a problem */ 
            if (received_bytes < 0) {
            return 0;
        }
        
            /* no problem, just no bytes */ 
            if (received_bytes == 0)
            return 0;
        
            /* the signature of a BACnet/IP packet */ 
            if (buf[0] != BVLL_TYPE_BACNET_IP)
            return 0;
        function_type = buf[1];
        
            /* decode the length of the PDU - length is inclusive of BVLC */ 
            (void) decode_unsigned16(&buf[2], &npdu_len);
        
            /* subtract off the BVLC header */ 
            npdu_len -= 4;
        switch (function_type)
             {
        case BVLC_RESULT:
            break;
        case BVLC_WRITE_BROADCAST_DISTRIBUTION_TABLE:
            
                /* Upon receipt of a BVLL Write-Broadcast-Distribution-Table
                   message, a BBMD shall attempt to create or replace its BDT,
                   depending on whether or not a BDT has previously existed.
                   If the creation or replacement of the BDT is successful, the BBMD
                   shall return a BVLC-Result message to the originating device with
                   a result code of X'0000'. Otherwise, the BBMD shall return a
                   BVLC-Result message to the originating device with a result code
                   of X'0010' indicating that the write attempt has failed. */ 
                break;
        case BVLC_READ_BROADCAST_DISTRIBUTION_TABLE:
            break;
        case BVLC_READ_BROADCAST_DISTRIBUTION_TABLE_ACK:
            break;
        case BVLC_FORWARDED_NPDU:
            
                /* Upon receipt of a BVLL Forwarded-NPDU message, a BBMD shall
                   process it according to whether it was received from a peer
                   BBMD as the result of a directed broadcast or a unicast
                   transmission. A BBMD may ascertain the method by which Forwarded-
                   NPDU messages will arrive by inspecting the broadcast distribution
                   mask field in its own BDT entry since all BDTs are required
                   to be identical. If the message arrived via directed broadcast,
                   it was also received by the other devices on the BBMD's subnet. In
                   this case the BBMD merely retransmits the message directly to each
                   foreign device currently in the BBMD's FDT. If the
                   message arrived via a unicast transmission it has not yet been
                   received by the other devices on the BBMD's subnet. In this case,
                   the message is sent to the devices on the BBMD's subnet using the
                   B/IP broadcast address as well as to each foreign device
                   currently in the BBMD's FDT. A BBMD on a subnet with no other
                   BACnet devices may omit the broadcast using the B/IP
                   broadcast address. The method by which a BBMD determines whether
                   or not other BACnet devices are present is a local matter. */ 
                bvlc_broadcast_npdu(&sin, &buf[4], npdu_len);
            bvlc_fdt_forward_npdu(&sin, &buf[4], npdu_len);
            break;
        case BVLC_REGISTER_FOREIGN_DEVICE:
            break;
        case BVLC_READ_FOREIGN_DEVICE_TABLE:
            break;
        case BVLC_READ_FOREIGN_DEVICE_TABLE_ACK:
            break;
        case BVLC_DELETE_FOREIGN_DEVICE_TABLE_ENTRY:
            break;
        case BVLC_DISTRIBUTE_BROADCAST_TO_NETWORK:
            bvlc_broadcast_forward_npdu(&sin, &buf[4], npdu_len);
            bvlc_fdt_forward_npdu(&sin, &buf[4], npdu_len);
            break;
        case BVLC_ORIGINAL_UNICAST_NPDU:
            
                /* ignore messages from me */ 
                if (sin.sin_addr.s_addr == BIP_Address.s_addr)
                npdu_len = 0;
            
            else {
                bvlc_internet_to_bacnet_address(src, &sin);
                
                    /* copy the buffer into the PDU */ 
                    if (npdu_len < max_npdu)
                    memmove(&npdu[0], &buf[4], npdu_len);
                
                    /* ignore packets that are too large */ 
                    /* clients should check my max-apdu first */ 
                    else
                    npdu_len = 0;
            }
            break;
        case BVLC_ORIGINAL_BROADCAST_NPDU:
            
                /* Upon receipt of a BVLL Original-Broadcast-NPDU message,
                   a BBMD shall construct a BVLL Forwarded-NPDU message and
                   send it to each IP subnet in its BDT with the exception
                   of its own. The B/IP address to which the Forwarded-NPDU
                   message is sent is formed by inverting the broadcast
                   distribution mask in the BDT entry and logically ORing it
                   with the BBMD address of the same entry. This process
                   produces either the directed broadcast address of the remote
                   subnet or the unicast address of the BBMD on that subnet
                   depending on the contents of the broadcast distribution
                   mask. See J.4.3.2.. In addition, the received BACnet NPDU
                   shall be sent directly to each foreign device currently in
                   the BBMD's FDT also using the BVLL Forwarded-NPDU message. */ 
                bvlc_internet_to_bacnet_address(src, &sin);
            
                /* copy the buffer into the PDU */ 
                if (npdu_len < max_npdu)
                memmove(&npdu[0], &buf[4], npdu_len);
            
                /* ignore packets that are too large */ 
                /* clients should check my max-apdu first */ 
                else
                npdu_len = 0;
            
                /* if BDT or FDT entries exist, Forward the NPDU */ 
                bvlc_bdt_forward_npdu(&sin, &buf[4], npdu_len);
            bvlc_fdt_forward_npdu(&sin, &buf[4], npdu_len);
            break;
        default:
            break;
            }
        return npdu_len;
    }
    
#ifdef TEST
#include <assert.h>
#include <string.h>
#include "ctest.h"
    void testBVLC(Test * pTest)  {
        (void) pTest;
    } 
#ifdef TEST_BBMD
    int main(void)  {
        Test * pTest;
        bool rc;
        pTest = ct_create("BACnet Virtual Link Control", NULL);
        
            /* individual tests */ 
            rc = ct_addTestFunction(pTest, testBVLC);
        assert(rc);
        
            /* configure output */ 
            ct_setStream(pTest, stdout);
        ct_run(pTest);
        (void) ct_report(pTest);
        ct_destroy(pTest);
        return 0;
    }
    
#endif  /* TEST_BBMD */
#endif  /* TEST */
