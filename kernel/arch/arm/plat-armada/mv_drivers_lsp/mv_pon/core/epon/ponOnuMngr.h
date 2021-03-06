/******************************************************************************/
/**                                                                          **/
/**  COPYRIGHT (C) 2000, 2001, Iamba Technologies Ltd.                       **/ 
/**--------------------------------------------------------------------------**/
/******************************************************************************/
/******************************************************************************/
/**                                                                          **/
/**  MODULE      : ONU GPON                                                  **/
/**                                                                          **/
/**  FILE        : ponOnuMngr.h                                              **/
/**                                                                          **/
/**  DESCRIPTION : This file contains ONU EPON Manager definitions           **/
/**                                                                          **/
/******************************************************************************
 *                                                                            *                              
 *  MODIFICATION HISTORY:                                                     *
 *                                                                            *
 *   26Jan10  oren_ben_hayun    created                                       *  
 * ========================================================================== *      
 *                                                                          
 ******************************************************************************/
#ifndef _ONU_EPON_MNGR_H
#define _ONU_EPON_MNGR_H

/* Include Files
------------------------------------------------------------------------------*/
 
/* Definitions
------------------------------------------------------------------------------*/ 
#define MPC_FRAME_LENGTH_MASK  (0x000000FF)
#define MPC_FRAME_LENGTH_SHIFT (0)

/* Enums                              
------------------------------------------------------------------------------*/ 

/* Typedefs
------------------------------------------------------------------------------*/

/* Global variables
------------------------------------------------------------------------------*/

/* Global functions
------------------------------------------------------------------------------*/
/* Interrupt handler Functions */
void onuEponPonMngIntrAlarmHandler(MV_U32 alarm, MV_BOOL alarmStatus); 
void onuEponPonMngIntrMessageHandler(MV_U32 msg);

/* Macros
------------------------------------------------------------------------------*/    

#endif /* _ONU_EPON_MNGR_H */
