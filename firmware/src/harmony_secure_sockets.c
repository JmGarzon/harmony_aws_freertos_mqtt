/* ************************************************************************** */
/** Descriptive File Name

  @Company
    Company Name

  @File Name
    filename.c

  @Summary
    Brief description of the file.

  @Description
 This replaces the aws_secure_sockets
 */
/* ************************************************************************** */

/** these extras might be moved to a seperate .h */

#define socklen_t uint32_t
#include <stdint.h>


struct freertos_sockaddr
{
	/* _HT_ On 32- and 64-bit architectures, the addition of the two uint8_t
	fields doesn't make the structure bigger, due to alignment.
	The fields are inserted as a preparation for IPv6. */

	/* sin_len and sin_family not used in the IPv4-only release. */
	uint8_t sin_len;		/* length of this structure. */
	uint8_t sin_family;		/* FREERTOS_AF_INET. */
	uint16_t sin_port;
	uint32_t sin_addr;
};


/*  */



/* FreeRTOS includes. */
#include "FreeRTOS.h"
//#include "FreeRTOSIPConfig.h"
#include "list.h"
//#include "FreeRTOS_IP.h"
//#include "FreeRTOS_Sockets.h"
#include "aws_secure_sockets.h"
#include "aws_tls.h"
#include "task.h"
#include "berkeley_api.h"




// addditional includes.

#include <string.h>             // string.h was not included by default?

/* Internal context structure. */
typedef struct SSOCKETContext
{
    Socket_t xSocket;
    char * pcDestination;
    void * pvTLSContext;
    BaseType_t xRequireTLS;
    BaseType_t xSendFlags;
    BaseType_t xRecvFlags;
    char * pcServerCertificate;
    uint32_t ulServerCertificateLength;
    char ** ppcAlpnProtocols;
    uint32_t ulAlpnProtocolsCount;
} SSOCKETContext_t, * SSOCKETContextPtr_t;

/*
 * Helper routines.
 */

/*
 * @brief Network send callback.
 */

static BaseType_t prvNetworkSend( void * pvContext,
                                  const char * pucData,
                                  //const unsigned char * pucData,
                                  size_t xDataLength )
{
    SSOCKETContextPtr_t pxContext = ( SSOCKETContextPtr_t ) pvContext; /*lint !e9087 cast used for portability. */

    //return FreeRTOS_send( pxContext->xSocket, pucData, xDataLength, pxContext->xSendFlags );
    
    return send( (int) pxContext->xSocket,  pucData   ,  xDataLength  ,pxContext->xSendFlags );
}

int     send( SOCKET s, const char* buf, int len, int flags );

/*-----------------------------------------------------------*/

/*
 * @brief Network receive callback.
 */
static BaseType_t prvNetworkRecv( void * pvContext,
                                  //  unsigned char * pucReceiveBuffer,
                                  char * pucReceiveBuffer,
                                  size_t xReceiveLength )
{
    SSOCKETContextPtr_t pxContext = ( SSOCKETContextPtr_t ) pvContext; /*lint !e9087 cast used for portability. */

    //return FreeRTOS_recv( pxContext->xSocket, pucReceiveBuffer, xReceiveLength, pxContext->xRecvFlags );
    return recv( (int) pxContext->xSocket, pucReceiveBuffer, xReceiveLength, pxContext->xRecvFlags );
}

/*-----------------------------------------------------------*/



/*

 * Interface routines.
 */

int32_t SOCKETS_Close( Socket_t xSocket )
{
    SSOCKETContextPtr_t pxContext = ( SSOCKETContextPtr_t ) xSocket; /*lint !e9087 cast used for portability. */
    uint32_t ulProtocol;

    if( NULL != pxContext )
    {
        /* Clean-up destination string. */
        if( NULL != pxContext->pcDestination )
        {
            vPortFree( pxContext->pcDestination );
        }

        /* Clean-up server certificate. */
        if( NULL != pxContext->pcServerCertificate )
        {
            vPortFree( pxContext->pcServerCertificate );
        }

        /* Clean-up application protocol array. */
        if( NULL != pxContext->ppcAlpnProtocols )
        {
            for( ulProtocol = 0; 
                 ulProtocol < pxContext->ulAlpnProtocolsCount; 
                 ulProtocol++ )
            {
                if( NULL != pxContext->ppcAlpnProtocols[ ulProtocol ] )
                {
                    vPortFree( pxContext->ppcAlpnProtocols[ ulProtocol ] );
                }
            }

            vPortFree( pxContext->ppcAlpnProtocols );
        }

        /* Clean-up TLS context. */
        if( pdTRUE == pxContext->xRequireTLS )
        {
            TLS_Cleanup( pxContext->pvTLSContext );
        }

        /* Close the underlying socket handle. */
        //( void ) FreeRTOS_closesocket( pxContext->xSocket );  // Close a socket. https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/API/close.html
        ( void ) closesocket( (int)pxContext->xSocket ); 
        /* Free the context. */
        vPortFree( pxContext );
    }

    return pdFREERTOS_ERRNO_NONE;
}
/*-----------------------------------------------------------*/

int32_t SOCKETS_Connect( Socket_t xSocket,
                         SocketsSockaddr_t * pxAddress,
                         socklen_t xAddressLength )
{
    int32_t lStatus = SOCKETS_ERROR_NONE;
    SSOCKETContextPtr_t pxContext = ( SSOCKETContextPtr_t ) xSocket; /*lint !e9087 cast used for portability. */
    TLSParams_t xTLSParams = { 0 };
    
    struct freertos_sockaddr xTempAddress = { 0 };

    if( ( pxContext != SOCKETS_INVALID_SOCKET ) && ( pxAddress != NULL ) )
    {
        /* Connect the wrapped socket. */
        xTempAddress.sin_addr = pxAddress->ulAddress;
        xTempAddress.sin_family = pxAddress->ucSocketDomain;
        xTempAddress.sin_len = ( uint8_t ) sizeof( xTempAddress );
        xTempAddress.sin_port = pxAddress->usPort;
        
        // lStatus = FreeRTOS_connect( pxContext->xSocket, &xTempAddress, xAddressLength );
        
       
        
        lStatus = connect( (int) pxContext->xSocket , (struct sockaddr*) &xTempAddress, xAddressLength);
           
        
    
    

        /* Negotiate TLS if requested. */
        if( ( SOCKETS_ERROR_NONE == lStatus ) && ( pdTRUE == pxContext->xRequireTLS ) )
        {
            xTLSParams.ulSize = sizeof( xTLSParams );
            xTLSParams.pcDestination = pxContext->pcDestination;
            xTLSParams.pcServerCertificate = pxContext->pcServerCertificate;
            xTLSParams.ulServerCertificateLength = pxContext->ulServerCertificateLength;
            xTLSParams.ppcAlpnProtocols = ( const char ** )pxContext->ppcAlpnProtocols;
            xTLSParams.ulAlpnProtocolsCount = pxContext->ulAlpnProtocolsCount;
            xTLSParams.pvCallerContext = pxContext;
            xTLSParams.pxNetworkRecv = prvNetworkRecv;
            xTLSParams.pxNetworkSend = prvNetworkSend;
            lStatus = TLS_Init( &pxContext->pvTLSContext, &xTLSParams );

            if( SOCKETS_ERROR_NONE == lStatus )
            {
                lStatus = TLS_Connect( pxContext->pvTLSContext );
            }
        }
    }
    else
    {
        lStatus = SOCKETS_SOCKET_ERROR;
    }

    return lStatus;
}
/*-----------------------------------------------------------*/

uint32_t SOCKETS_GetHostByName( const char * pcHostName )
{
    //return FreeRTOS_gethostbyname( pcHostName );    // Performs a Domain Name System (DNS) lookup on a host name
                                                    // https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_TP/API/gethostbyname.html
    return gethostbyname( pcHostName ); 
}
/*-----------------------------------------------------------*/

int32_t SOCKETS_Recv( Socket_t xSocket,
                      void * pvBuffer,
                      size_t xBufferLength,
                      uint32_t ulFlags )
{
    int32_t lStatus = SOCKETS_SOCKET_ERROR;
    SSOCKETContextPtr_t pxContext = ( SSOCKETContextPtr_t ) xSocket; /*lint !e9087 cast used for portability. */

    pxContext->xRecvFlags = ( BaseType_t ) ulFlags;

    if( ( xSocket != SOCKETS_INVALID_SOCKET ) &&
        ( pvBuffer != NULL ) )
    {
        if( pdTRUE == pxContext->xRequireTLS )
        {
            /* Receive through TLS pipe, if negotiated. */
            lStatus = TLS_Recv( pxContext->pvTLSContext, pvBuffer, xBufferLength );
        }
        else
        {
            /* Receive unencrypted. */
            lStatus = prvNetworkRecv( pxContext, pvBuffer, xBufferLength );
        }
    }

    return lStatus;
}
/*-----------------------------------------------------------*/

int32_t SOCKETS_Send( Socket_t xSocket,
                      const void * pvBuffer,
                      size_t xDataLength,
                      uint32_t ulFlags )
{
    int32_t lStatus = SOCKETS_SOCKET_ERROR;
    SSOCKETContextPtr_t pxContext = ( SSOCKETContextPtr_t ) xSocket; /*lint !e9087 cast used for portability. */

    if( ( xSocket != SOCKETS_INVALID_SOCKET ) &&
        ( pvBuffer != NULL ) )
    {
        pxContext->xSendFlags = ( BaseType_t ) ulFlags;

        if( pdTRUE == pxContext->xRequireTLS )
        {
            /* Send through TLS pipe, if negotiated. */
            lStatus = TLS_Send( pxContext->pvTLSContext, pvBuffer, xDataLength );
        }
        else
        {
            /* Send unencrypted. */
            lStatus = prvNetworkSend( pxContext, pvBuffer, xDataLength );
        }
    }

    return lStatus;
}
/*-----------------------------------------------------------*/

int32_t SOCKETS_SetSockOpt( Socket_t xSocket,
                            int32_t lLevel,
                            int32_t lOptionName,
                            const void * pvOptionValue,
                            size_t xOptionLength )
{
    int32_t lStatus = pdFREERTOS_ERRNO_NONE;
    TickType_t xTimeout;
    SSOCKETContextPtr_t pxContext = ( SSOCKETContextPtr_t ) xSocket; /*lint !e9087 cast used for portability. */
    char ** ppcAlpnIn = ( char ** )pvOptionValue;
    size_t xLength = 0;
    uint32_t ulProtocol;
    
    switch( lOptionName )
    {
        case SOCKETS_SO_SERVER_NAME_INDICATION:

            /* Non-NULL destination string indicates that SNI extension should
             * be used during TLS negotiation. */
            if( NULL == ( pxContext->pcDestination =
                              ( char * ) pvPortMalloc( 1U + xOptionLength ) ) )
            {
                lStatus = pdFREERTOS_ERRNO_ENOMEM;
            }
            else
            {
                memcpy( pxContext->pcDestination, pvOptionValue, xOptionLength );
                pxContext->pcDestination[ xOptionLength ] = '\0';
            }

            break;

        case SOCKETS_SO_TRUSTED_SERVER_CERTIFICATE:

            /* Non-NULL server certificate field indicates that the default trust
             * list should not be used. */
            if( NULL == ( pxContext->pcServerCertificate =
                              ( char * ) pvPortMalloc( xOptionLength ) ) )
            {
                lStatus = pdFREERTOS_ERRNO_ENOMEM;
            }
            else
            {
                memcpy( pxContext->pcServerCertificate, pvOptionValue, xOptionLength );
                pxContext->ulServerCertificateLength = xOptionLength;
            }

            break;

        case SOCKETS_SO_REQUIRE_TLS:
            pxContext->xRequireTLS = pdTRUE;
            break;

        case SOCKETS_SO_ALPN_PROTOCOLS:
            /* Allocate a sufficiently long array of pointers. */
            pxContext->ulAlpnProtocolsCount = 1 + xOptionLength;
            if( NULL == ( pxContext->ppcAlpnProtocols =
                ( char ** )pvPortMalloc( pxContext->ulAlpnProtocolsCount ) ) )
            {
                lStatus = pdFREERTOS_ERRNO_ENOMEM;
            }
            else
            {
                pxContext->ppcAlpnProtocols[ 
                    pxContext->ulAlpnProtocolsCount - 1 ] = NULL;
            }

            /* Copy each protocol string. */
            for( ulProtocol = 0; 
                 ( ulProtocol < pxContext->ulAlpnProtocolsCount - 1 ) &&
                    ( pdFREERTOS_ERRNO_NONE == lStatus );       
                 ulProtocol++ )
            {
                xLength = strlen( ppcAlpnIn[ ulProtocol ] );
                if( NULL == ( pxContext->ppcAlpnProtocols[ ulProtocol ] = 
                    ( char * )pvPortMalloc( 1 + xLength ) ) )
                {
                    lStatus = pdFREERTOS_ERRNO_ENOMEM;
                }
                else
                {
                    memcpy( pxContext->ppcAlpnProtocols[ ulProtocol ], 
                            ppcAlpnIn[ ulProtocol ],
                            xLength );
                    pxContext->ppcAlpnProtocols[ ulProtocol ][ xLength ] = '\0';
                }
            }
            break;

        case SOCKETS_SO_NONBLOCK:
            xTimeout = 0;
   //         lStatus = FreeRTOS_setsockopt( pxContext->xSocket,
   //                                        lLevel,
   //                                        SOCKETS_SO_RCVTIMEO,
   //                                        &xTimeout,
   //                                        sizeof( xTimeout ) );
            lStatus =   setsockopt( pxContext->xSocket,
                                           lLevel,
                                           SOCKETS_SO_RCVTIMEO,
                                           &xTimeout,
                                           sizeof( xTimeout ) );

            if( lStatus == SOCKETS_ERROR_NONE )
            {
                //lStatus = FreeRTOS_setsockopt( pxContext->xSocket,
                //                               lLevel,
                //                               SOCKETS_SO_SNDTIMEO,
                 //                              &xTimeout,
                 //                              sizeof( xTimeout ) );
                
                lStatus = setsockopt( pxContext->xSocket,
                                               lLevel,
                                               SOCKETS_SO_SNDTIMEO,
                                               &xTimeout,
                                               sizeof( xTimeout ) );
            }

            break;

        case SOCKETS_SO_RCVTIMEO:
        case SOCKETS_SO_SNDTIMEO:
            /* Comply with Berkeley standard - a 0 timeout is wait forever. */
            xTimeout = *( ( const TickType_t * ) pvOptionValue ); /*lint !e9087 pvOptionValue passed should be of TickType_t */

            if( xTimeout == 0U )
            {
                xTimeout = portMAX_DELAY;
            }

            //lStatus = FreeRTOS_setsockopt( pxContext->xSocket,
            //                               lLevel,
            //                               lOptionName,
            //                               &xTimeout,
            //                               xOptionLength );
            lStatus = setsockopt( pxContext->xSocket,
                                           lLevel,
                                           lOptionName,
                                           &xTimeout,
                                           xOptionLength );
            break;

        default:
            //lStatus = FreeRTOS_setsockopt( pxContext->xSocket,
            //                               lLevel,
            //                               lOptionName,
            //                               pvOptionValue,
            //                              xOptionLength );
            lStatus = setsockopt( pxContext->xSocket,
                                           lLevel,
                                           lOptionName,
                                           pvOptionValue,
                                           xOptionLength );
            break;
    }

    return lStatus;
}
/*-----------------------------------------------------------*/

int32_t SOCKETS_Shutdown( Socket_t xSocket,
                          uint32_t ulHow )
{
    SSOCKETContextPtr_t pxContext = ( SSOCKETContextPtr_t ) xSocket; /*lint !e9087 cast used for portability. */

    //return FreeRTOS_shutdown( pxContext->xSocket, ( BaseType_t ) ulHow );
    // https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/API/shutdown.html
    // Disable reads and writes on a connected TCP socket. A connected TCP socket must be gracefully shut down before it can be closed.
    return FreeRTOS_shutdown( (int) pxContext->xSocket, ( BaseType_t ) ulHow );
}
/*-----------------------------------------------------------*/

Socket_t SOCKETS_Socket( int32_t lDomain,
                         int32_t lType,
                         int32_t lProtocol )
{
    int32_t lStatus = SOCKETS_ERROR_NONE;
    SSOCKETContextPtr_t pxContext = NULL;
    Socket_t xSocket;

    /* Create the wrapped socket. */
    if( FREERTOS_INVALID_SOCKET == /*lint !e923 cast used for portability. */
    //    ( xSocket = FreeRTOS_socket( lDomain, lType, lProtocol ) ) )
        ( xSocket = socket( lDomain, lType, lProtocol ) ) )
    {
        lStatus = SOCKETS_SOCKET_ERROR;
    }

    if( lStatus == SOCKETS_ERROR_NONE )
    {
        /* Allocate the internal context structure. */
        if( NULL == ( pxContext = pvPortMalloc( sizeof( SSOCKETContext_t ) ) ) )
        {
            /* Need to close socket. */
            SOCKETS_Close( xSocket );
            lStatus = SOCKETS_ENOMEM;
        }
        else
        {
            memset( pxContext, 0, sizeof( SSOCKETContext_t ) );
            pxContext->xSocket = xSocket;
        }
    }

    return pxContext;
}
/*-----------------------------------------------------------*/

BaseType_t SOCKETS_Init( void )
{
    /* Empty initialization for Visual Studio board. */
    return pdPASS;
}
