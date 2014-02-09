/*
 * Copyright © 2008 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Soft-
 * ware"), to deal in the Software without restriction, including without
 * limitation the rights to use, copy, modify, merge, publish, distribute,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, provided that the above copyright
 * notice(s) and this permission notice appear in all copies of the Soft-
 * ware and that both the above copyright notice(s) and this permission
 * notice appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABIL-
 * ITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY
 * RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN
 * THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSE-
 * QUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFOR-
 * MANCE OF THIS SOFTWARE.
 *
 * Except as contained in this notice, the name of a copyright holder shall
 * not be used in advertising or otherwise to promote the sale, use or
 * other dealings in this Software without prior written authorization of
 * the copyright holder.
 *
 * Authors:
 *   Kristian Høgsberg (krh@redhat.com)
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#define NEED_REPLIES
#include <stdio.h>
#include <stdint.h>
#include <X11/Xlibint.h>
#include <X11/extensions/Xext.h>
#include <X11/extensions/extutil.h>
#include <X11/extensions/dri2proto.h>
#include <drm.h>
#include <xf86drm.h>
#include "list.h"
#include "X11/extensions/dri2.h"

/* Allow the build to work with an older versions of dri2proto.h and
 * dri2tokens.h.
 */
#if DRI2_MINOR < 1
#undef DRI2_MINOR
#define DRI2_MINOR 1
#define X_DRI2GetBuffersWithFormat 7
#endif


static char dri2ExtensionName[] = DRI2_NAME;
static XExtensionInfo *dri2Info;
static XEXT_GENERATE_CLOSE_DISPLAY (DRI2CloseDisplay2, dri2Info)

/**
 * List of per-Display privates..
 */
static struct list dpy_list = { &dpy_list, &dpy_list };

typedef struct {
	struct list list;
	Display *dpy;
	const DRI2EventOps *ops;
} DRI2Display;

static DRI2Display * dpy2dri(Display *dpy)
{
	DRI2Display *dri2dpy;
	list_for_each_entry(dri2dpy, &dpy_list, list) {
		if (dri2dpy->dpy == dpy) {
			return dri2dpy;
		}
	}
	return NULL;
}

static int
DRI2CloseDisplay(Display *dpy, XExtCodes *codes)
{
	DRI2Display *dri2dpy = dpy2dri(dpy);
	if (dri2dpy) {
		list_del(&dri2dpy->list);
		free(dri2dpy);
	}
	return DRI2CloseDisplay2(dpy, codes);
}

Bool
DRI2InitDisplay(Display *dpy, const DRI2EventOps *ops)
{
	DRI2Display *dri2dpy = dpy2dri(dpy);
	if (!dri2dpy) {
		dri2dpy = malloc(sizeof(*dri2dpy));
		if (!dri2dpy) {
			return False;
		}
		dri2dpy->dpy = dpy;
		dri2dpy->ops = ops;
		list_add(&dri2dpy->list, &dpy_list);
	}
	return True;
}

static Bool
DRI2WireToEvent(Display *dpy, XEvent *event, xEvent *wire);
static Status
DRI2EventToWire(Display *dpy, XEvent *event, xEvent *wire);
static int
DRI2Error(Display *display, xError *err, XExtCodes *codes, int *ret_code);

static /* const */ XExtensionHooks dri2ExtensionHooks = {
  NULL,                   /* create_gc */
  NULL,                   /* copy_gc */
  NULL,                   /* flush_gc */
  NULL,                   /* free_gc */
  NULL,                   /* create_font */
  NULL,                   /* free_font */
  DRI2CloseDisplay,       /* close_display */
  DRI2WireToEvent,        /* wire_to_event */
  DRI2EventToWire,        /* event_to_wire */
  DRI2Error,              /* error */
  NULL,                   /* error_string */
};

static XEXT_GENERATE_FIND_DISPLAY (DRI2FindDisplay,
                                   dri2Info,
                                   dri2ExtensionName,
                                   &dri2ExtensionHooks,
                                   0, NULL)

static Bool
DRI2WireToEvent(Display *dpy, XEvent *event, xEvent *wire)
{
   XExtDisplayInfo *info = DRI2FindDisplay(dpy);
   DRI2Display *dri2dpy = dpy2dri(dpy);

   XextCheckExtension(dpy, info, dri2ExtensionName, False);

   if (dri2dpy && dri2dpy->ops && dri2dpy->ops->WireToEvent) {
	   return dri2dpy->ops->WireToEvent(dpy, info, event, wire);
   }

   return False;
}

/* We don't actually support this.  It doesn't make sense for clients to
 * send each other DRI2 events.
 */
static Status
DRI2EventToWire(Display *dpy, XEvent *event, xEvent *wire)
{
   XExtDisplayInfo *info = DRI2FindDisplay(dpy);
   DRI2Display *dri2dpy = dpy2dri(dpy);

   XextCheckExtension(dpy, info, dri2ExtensionName, False);

   if (dri2dpy && dri2dpy->ops && dri2dpy->ops->EventToWire) {
	   return dri2dpy->ops->EventToWire(dpy, info, event, wire);
   }

   return Success;
}

static int
DRI2Error(Display *dpy, xError *err, XExtCodes *codes, int *ret_code)
{
	DRI2Display *dri2dpy = dpy2dri(dpy);

	if (dri2dpy && dri2dpy->ops && dri2dpy->ops->Error) {
		return dri2dpy->ops->Error(dpy, err, codes, ret_code);
	}

    return False;
}

Bool
DRI2QueryExtension(Display * dpy, int *eventBase, int *errorBase)
{
   XExtDisplayInfo *info = DRI2FindDisplay(dpy);

   if (XextHasExtension(info)) {
      *eventBase = info->codes->first_event;
      *errorBase = info->codes->first_error;
      return True;
   }

   return False;
}

Bool
DRI2QueryVersion(Display * dpy, int *major, int *minor)
{
   XExtDisplayInfo *info = DRI2FindDisplay(dpy);
   xDRI2QueryVersionReply rep;
   xDRI2QueryVersionReq *req;
   int i, nevents;

   XextCheckExtension(dpy, info, dri2ExtensionName, False);

   LockDisplay(dpy);
   GetReq(DRI2QueryVersion, req);
   req->reqType = info->codes->major_opcode;
   req->dri2ReqType = X_DRI2QueryVersion;
   req->majorVersion = DRI2_MAJOR;
   req->minorVersion = DRI2_MINOR;
   if (!_XReply(dpy, (xReply *) & rep, 0, xFalse)) {
      UnlockDisplay(dpy);
      SyncHandle();
      return False;
   }
   *major = rep.majorVersion;
   *minor = rep.minorVersion;
   UnlockDisplay(dpy);
   SyncHandle();

   switch (rep.minorVersion) {
   case 1:
	   nevents = 0;
	   break;
   case 2:
	   nevents = 1;
	   break;
   case 3:
   default:
	   nevents = 2;
	   break;
   }
	
   for (i = 0; i < nevents; i++) {
       XESetWireToEvent (dpy, info->codes->first_event + i, DRI2WireToEvent);
       XESetEventToWire (dpy, info->codes->first_event + i, DRI2EventToWire);
   }

   return True;
}

Bool
DRI2Connect(Display * dpy, XID window,
		int driverType, char **driverName, char **deviceName)
{
   XExtDisplayInfo *info = DRI2FindDisplay(dpy);
   xDRI2ConnectReply rep;
   xDRI2ConnectReq *req;

   XextCheckExtension(dpy, info, dri2ExtensionName, False);

   LockDisplay(dpy);
   GetReq(DRI2Connect, req);
   req->reqType = info->codes->major_opcode;
   req->dri2ReqType = X_DRI2Connect;
   req->window = window;
   req->driverType = driverType;
   if (!_XReply(dpy, (xReply *) & rep, 0, xFalse)) {
      UnlockDisplay(dpy);
      SyncHandle();
      return False;
   }

   if (rep.driverNameLength == 0 && rep.deviceNameLength == 0) {
      UnlockDisplay(dpy);
      SyncHandle();
      return False;
   }

   *driverName = Xmalloc(rep.driverNameLength + 1);
   if (*driverName == NULL) {
      _XEatData(dpy,
                ((rep.driverNameLength + 3) & ~3) +
                ((rep.deviceNameLength + 3) & ~3));
      UnlockDisplay(dpy);
      SyncHandle();
      return False;
   }
   _XReadPad(dpy, *driverName, rep.driverNameLength);
   (*driverName)[rep.driverNameLength] = '\0';

   *deviceName = Xmalloc(rep.deviceNameLength + 1);
   if (*deviceName == NULL) {
      Xfree(*driverName);
      _XEatData(dpy, ((rep.deviceNameLength + 3) & ~3));
      UnlockDisplay(dpy);
      SyncHandle();
      return False;
   }
   _XReadPad(dpy, *deviceName, rep.deviceNameLength);
   (*deviceName)[rep.deviceNameLength] = '\0';

   UnlockDisplay(dpy);
   SyncHandle();

   return True;
}

Bool
DRI2Authenticate(Display * dpy, XID window, drm_magic_t magic)
{
   XExtDisplayInfo *info = DRI2FindDisplay(dpy);
   xDRI2AuthenticateReq *req;
   xDRI2AuthenticateReply rep;

   XextCheckExtension(dpy, info, dri2ExtensionName, False);

   LockDisplay(dpy);
   GetReq(DRI2Authenticate, req);
   req->reqType = info->codes->major_opcode;
   req->dri2ReqType = X_DRI2Authenticate;
   req->window = window;
   req->magic = magic;

   if (!_XReply(dpy, (xReply *) & rep, 0, xFalse)) {
      UnlockDisplay(dpy);
      SyncHandle();
      return False;
   }

   UnlockDisplay(dpy);
   SyncHandle();

   return rep.authenticated;
}

void
DRI2CreateDrawable(Display * dpy, XID drawable)
{
   XExtDisplayInfo *info = DRI2FindDisplay(dpy);
   xDRI2CreateDrawableReq *req;

   XextSimpleCheckExtension(dpy, info, dri2ExtensionName);

   LockDisplay(dpy);
   GetReq(DRI2CreateDrawable, req);
   req->reqType = info->codes->major_opcode;
   req->dri2ReqType = X_DRI2CreateDrawable;
   req->drawable = drawable;
   UnlockDisplay(dpy);
   SyncHandle();
}

void
DRI2DestroyDrawable(Display * dpy, XID drawable)
{
   XExtDisplayInfo *info = DRI2FindDisplay(dpy);
   xDRI2DestroyDrawableReq *req;

   XextSimpleCheckExtension(dpy, info, dri2ExtensionName);

   XSync(dpy, False);

   LockDisplay(dpy);
   GetReq(DRI2DestroyDrawable, req);
   req->reqType = info->codes->major_opcode;
   req->dri2ReqType = X_DRI2DestroyDrawable;
   req->drawable = drawable;
   UnlockDisplay(dpy);
   SyncHandle();
}

static DRI2Buffer *
getbuffers(Display *dpy, XID drawable, int *width, int *height,
               unsigned int *attachments, int count, int *outCount, int dri2ReqType)
{
   XExtDisplayInfo *info = DRI2FindDisplay(dpy);
   xDRI2GetBuffersReply rep;
   xDRI2GetBuffersReq *req;
   DRI2Buffer *buffers;
   xDRI2Buffer repBuffer;
   CARD32 *p;
   int i, nattachments;

   /* DRI2GetBuffersWithFormat has interleaved attachment+format in
    * attachments[] array, so length of array is 2x as long..
    */
   if (dri2ReqType == X_DRI2GetBuffersWithFormat) {
          nattachments = 2 * count;
   } else {
          nattachments = count;
   }

   XextCheckExtension(dpy, info, dri2ExtensionName, False);

   LockDisplay(dpy);
   GetReqExtra(DRI2GetBuffers, nattachments * 4, req);
   req->reqType = info->codes->major_opcode;
   req->dri2ReqType = dri2ReqType;
   req->drawable = drawable;
   req->count = count;
   p = (CARD32 *) & req[1];
   for (i = 0; i < nattachments; i++)
      p[i] = attachments[i];

   if (!_XReply(dpy, (xReply *) & rep, 0, xFalse)) {
      UnlockDisplay(dpy);
      SyncHandle();
      return NULL;
   }

   *width = rep.width;
   *height = rep.height;
   *outCount = rep.count;

   buffers = calloc(rep.count, sizeof buffers[0]);
   for (i = 0; i < rep.count; i++) {
      _XReadPad(dpy, (char *) &repBuffer, sizeof repBuffer);
      if (buffers) {
         buffers[i].attachment = repBuffer.attachment;
         buffers[i].names[0] = repBuffer.name;
         buffers[i].pitch[0] = repBuffer.pitch;
         buffers[i].cpp = repBuffer.cpp;
         buffers[i].flags = repBuffer.flags;
      }
   }

   UnlockDisplay(dpy);
   SyncHandle();

   return buffers;
}

DRI2Buffer *
DRI2GetBuffers(Display * dpy, XID drawable,
               int *width, int *height,
               unsigned int *attachments, int count, int *outCount)
{
       return getbuffers(dpy, drawable, width, height, attachments,
                       count, outCount, X_DRI2GetBuffers);
}

DRI2Buffer *
DRI2GetBuffersWithFormat(Display * dpy, XID drawable,
                         int *width, int *height,
                         unsigned int *attachments, int count, int *outCount)
{
       return getbuffers(dpy, drawable, width, height, attachments,
                       count, outCount, X_DRI2GetBuffersWithFormat);
}


void
DRI2CopyRegion(Display * dpy, XID drawable, XserverRegion region,
               CARD32 dest, CARD32 src)
{
   XExtDisplayInfo *info = DRI2FindDisplay(dpy);
   xDRI2CopyRegionReq *req;
   xDRI2CopyRegionReply rep;

   XextSimpleCheckExtension(dpy, info, dri2ExtensionName);

   LockDisplay(dpy);
   GetReq(DRI2CopyRegion, req);
   req->reqType = info->codes->major_opcode;
   req->dri2ReqType = X_DRI2CopyRegion;
   req->drawable = drawable;
   req->region = region;
   req->dest = dest;
   req->src = src;

   _XReply(dpy, (xReply *) & rep, 0, xFalse);

   UnlockDisplay(dpy);
   SyncHandle();
}

#ifdef X_DRI2SwapBuffers
static void
load_swap_req(xDRI2SwapBuffersReq *req, CARD64 target, CARD64 divisor,
	     CARD64 remainder)
{
    req->target_msc_hi = target >> 32;
    req->target_msc_lo = target & 0xffffffff;
    req->divisor_hi = divisor >> 32;
    req->divisor_lo = divisor & 0xffffffff;
    req->remainder_hi = remainder >> 32;
    req->remainder_lo = remainder & 0xffffffff;
}

static CARD64
vals_to_card64(CARD32 lo, CARD32 hi)
{
    return (CARD64)hi << 32 | lo;
}

void DRI2SwapBuffers(Display *dpy, XID drawable, CARD64 target_msc,
		     CARD64 divisor, CARD64 remainder, CARD64 *count)
{
    XExtDisplayInfo *info = DRI2FindDisplay(dpy);
    xDRI2SwapBuffersReq *req;
    xDRI2SwapBuffersReply rep;

    XextSimpleCheckExtension (dpy, info, dri2ExtensionName);

    LockDisplay(dpy);
    GetReq(DRI2SwapBuffers, req);
    req->reqType = info->codes->major_opcode;
    req->dri2ReqType = X_DRI2SwapBuffers;
    req->drawable = drawable;
    load_swap_req(req, target_msc, divisor, remainder);

    _XReply(dpy, (xReply *)&rep, 0, xFalse);

    *count = vals_to_card64(rep.swap_lo, rep.swap_hi);

    UnlockDisplay(dpy);
    SyncHandle();
}
#endif

#ifdef X_DRI2GetMSC
Bool DRI2GetMSC(Display *dpy, XID drawable, CARD64 *ust, CARD64 *msc,
		CARD64 *sbc)
{
    XExtDisplayInfo *info = DRI2FindDisplay(dpy);
    xDRI2GetMSCReq *req;
    xDRI2MSCReply rep;

    XextCheckExtension (dpy, info, dri2ExtensionName, False);

    LockDisplay(dpy);
    GetReq(DRI2GetMSC, req);
    req->reqType = info->codes->major_opcode;
    req->dri2ReqType = X_DRI2GetMSC;
    req->drawable = drawable;

    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
	UnlockDisplay(dpy);
	SyncHandle();
	return False;
    }

    *ust = vals_to_card64(rep.ust_lo, rep.ust_hi);
    *msc = vals_to_card64(rep.msc_lo, rep.msc_hi);
    *sbc = vals_to_card64(rep.sbc_lo, rep.sbc_hi);

    UnlockDisplay(dpy);
    SyncHandle();

    return True;
}
#endif

#ifdef X_DRI2WaitMSC
static void
load_msc_req(xDRI2WaitMSCReq *req, CARD64 target, CARD64 divisor,
	     CARD64 remainder)
{
    req->target_msc_hi = target >> 32;
    req->target_msc_lo = target & 0xffffffff;
    req->divisor_hi = divisor >> 32;
    req->divisor_lo = divisor & 0xffffffff;
    req->remainder_hi = remainder >> 32;
    req->remainder_lo = remainder & 0xffffffff;
}

Bool DRI2WaitMSC(Display *dpy, XID drawable, CARD64 target_msc, CARD64 divisor,
		 CARD64 remainder, CARD64 *ust, CARD64 *msc, CARD64 *sbc)
{
    XExtDisplayInfo *info = DRI2FindDisplay(dpy);
    xDRI2WaitMSCReq *req;
    xDRI2MSCReply rep;

    XextCheckExtension (dpy, info, dri2ExtensionName, False);

    LockDisplay(dpy);
    GetReq(DRI2WaitMSC, req);
    req->reqType = info->codes->major_opcode;
    req->dri2ReqType = X_DRI2WaitMSC;
    req->drawable = drawable;
    load_msc_req(req, target_msc, divisor, remainder);

    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
	UnlockDisplay(dpy);
	SyncHandle();
	return False;
    }

    *ust = ((CARD64)rep.ust_hi << 32) | (CARD64)rep.ust_lo;
    *msc = ((CARD64)rep.msc_hi << 32) | (CARD64)rep.msc_lo;
    *sbc = ((CARD64)rep.sbc_hi << 32) | (CARD64)rep.sbc_lo;

    UnlockDisplay(dpy);
    SyncHandle();

    return True;
}
#endif

#ifdef X_DRI2WaitSBC
static void
load_sbc_req(xDRI2WaitSBCReq *req, CARD64 target)
{
    req->target_sbc_hi = target >> 32;
    req->target_sbc_lo = target & 0xffffffff;
}

Bool DRI2WaitSBC(Display *dpy, XID drawable, CARD64 target_sbc, CARD64 *ust,
		 CARD64 *msc, CARD64 *sbc)
{
    XExtDisplayInfo *info = DRI2FindDisplay(dpy);
    xDRI2WaitSBCReq *req;
    xDRI2MSCReply rep;

    XextCheckExtension (dpy, info, dri2ExtensionName, False);

    LockDisplay(dpy);
    GetReq(DRI2WaitSBC, req);
    req->reqType = info->codes->major_opcode;
    req->dri2ReqType = X_DRI2WaitSBC;
    req->drawable = drawable;
    load_sbc_req(req, target_sbc);

    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
	UnlockDisplay(dpy);
	SyncHandle();
	return False;
    }

    *ust = ((CARD64)rep.ust_hi << 32) | rep.ust_lo;
    *msc = ((CARD64)rep.msc_hi << 32) | rep.msc_lo;
    *sbc = ((CARD64)rep.sbc_hi << 32) | rep.sbc_lo;

    UnlockDisplay(dpy);
    SyncHandle();

    return True;
}
#endif

#ifdef X_DRI2SwapInterval
void DRI2SwapInterval(Display *dpy, XID drawable, int interval)
{
    XExtDisplayInfo *info = DRI2FindDisplay(dpy);
    xDRI2SwapIntervalReq *req;

    XextSimpleCheckExtension (dpy, info, dri2ExtensionName);

    LockDisplay(dpy);
    GetReq(DRI2SwapInterval, req);
    req->reqType = info->codes->major_opcode;
    req->dri2ReqType = X_DRI2SwapInterval;
    req->drawable = drawable;
    req->interval = interval;
    UnlockDisplay(dpy);
    SyncHandle();
}
#endif