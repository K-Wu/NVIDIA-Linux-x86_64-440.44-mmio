/*
 * Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "nvidia-drm-conftest.h" /* NV_DRM_ATOMIC_MODESET_AVAILABLE */

#if defined(NV_DRM_ATOMIC_MODESET_AVAILABLE)

#include "nvidia-drm-priv.h"
#include "nvidia-drm-modeset.h"
#include "nvidia-drm-crtc.h"
#include "nvidia-drm-os-interface.h"
#include "nvidia-drm-helper.h"

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc.h>

struct nv_drm_atomic_state {
    struct NvKmsKapiRequestedModeSetConfig config;
    struct drm_atomic_state base;
};

static inline struct nv_drm_atomic_state *to_nv_atomic_state(
    struct drm_atomic_state *state)
{
    return container_of(state, struct nv_drm_atomic_state, base);
}

struct drm_atomic_state *nv_drm_atomic_state_alloc(struct drm_device *dev)
{
    struct nv_drm_atomic_state *nv_state =
            nv_drm_calloc(1, sizeof(*nv_state));

    if (nv_state == NULL || drm_atomic_state_init(dev, &nv_state->base) < 0) {
        nv_drm_free(nv_state);
        return NULL;
    }

    return &nv_state->base;
}

void nv_drm_atomic_state_clear(struct drm_atomic_state *state)
{
    drm_atomic_state_default_clear(state);
}

void nv_drm_atomic_state_free(struct drm_atomic_state *state)
{
    struct nv_drm_atomic_state *nv_state =
                    to_nv_atomic_state(state);
    drm_atomic_state_default_release(state);
    nv_drm_free(nv_state);
}

/**
 * __will_generate_flip_event - Check whether event is going to be generated by
 * hardware when it flips from old crtc/plane state to current one. This
 * function is called after drm_atomic_helper_swap_state(), therefore new state
 * is swapped into current state.
 */
static bool __will_generate_flip_event(struct drm_crtc *crtc,
                                       struct drm_crtc_state *old_crtc_state)
{
    struct drm_crtc_state *new_crtc_state = crtc->state;
    struct drm_plane *primary_plane = crtc->primary;
    struct drm_plane_state *new_primary_plane_state = primary_plane->state,
        /* If plane is not affected by atomic commit, then old == new */
        *old_primary_plane_state = new_primary_plane_state;
    struct drm_plane *plane;
    struct drm_plane_state *plane_state;
    int i;

    if (!old_crtc_state->active  && !new_crtc_state->active) {
        /*
         * crtc is not active in old and new states therefore all planes are
         * disabled, hardware can not generate flip events.
         */
        return false;
    }

    nv_drm_for_each_plane_in_state(old_crtc_state->state,
        plane, plane_state, i) {
        if (plane == primary_plane) {
            old_primary_plane_state = plane_state;
            break;
        }
    }

    /*
     * Hardware generates flip event for only those
     * planes which were active previously.
     */
    return old_crtc_state->active &&
           old_primary_plane_state->fb != NULL;
}

/**
 * nv_drm_atomic_commit - validate/commit modeset config
 * @dev: DRM device
 * @state: atomic state tracking atomic update
 * @commit: commit/check modeset config associated with atomic update
 *
 * @state tracks atomic update and modeset objects affected
 * by the atomic update, but the state of the modeset objects it contains
 * depends on the current stage of the update.
 * At the commit stage, the proposed state is already stored in the current
 * state, and @state contains old state for all affected modeset objects.
 * At the check/validation stage, @state contains the proposed state for
 * all affected objects.
 *
 * Sequence of atomic update -
 *   1. The check/validation of proposed atomic state,
 *   2. Do any other steps that might fail,
 *   3. Put the proposed state into the current state pointers,
 *   4. Actually commit the hardware state,
 *   5. Cleanup old state.
 *
 * The function nv_drm_atomic_apply_modeset_config() is getting called
 * at stages (1) and (4) after drm_atomic_helper_swap_state().
 */
static int
nv_drm_atomic_apply_modeset_config(struct drm_device *dev,
                                   struct drm_atomic_state *state,
                                   bool commit)
{
    struct nv_drm_device *nv_dev = to_nv_device(dev);
    struct NvKmsKapiRequestedModeSetConfig *requested_config =
        &(to_nv_atomic_state(state)->config);
    struct drm_crtc *crtc;
    struct drm_crtc_state *crtc_state;
    int i;

    memset(requested_config, 0, sizeof(*requested_config));

    /* Loop over affected crtcs and construct NvKmsKapiRequestedModeSetConfig */
    nv_drm_for_each_crtc_in_state(state, crtc, crtc_state, i) {
        /*
         * When commiting a state, the new state is already stored in
         * crtc->state. When checking a proposed state, the proposed state is
         * stored in crtc_state.
         */
        struct drm_crtc_state *new_crtc_state =
                               commit ? crtc->state : crtc_state;
        struct nv_drm_crtc *nv_crtc = to_nv_crtc(crtc);

        requested_config->headRequestedConfig[nv_crtc->head] =
            to_nv_crtc_state(new_crtc_state)->req_config;

        requested_config->headsMask |= 1 << nv_crtc->head;

        if (commit) {
            struct drm_crtc_state *old_crtc_state = crtc_state;
            struct nv_drm_crtc_state *nv_new_crtc_state =
                to_nv_crtc_state(new_crtc_state);

            nv_new_crtc_state->nv_flip->event = new_crtc_state->event;
            new_crtc_state->event = NULL;

            /*
             * If flip event will be generated by hardware
             * then defer flip object processing to flip event from hardware.
             */
            if (__will_generate_flip_event(crtc, old_crtc_state)) {
                nv_drm_crtc_enqueue_flip(nv_crtc,
                                         nv_new_crtc_state->nv_flip);

                nv_new_crtc_state->nv_flip = NULL;
            }
        }
    }

    if (commit && nvKms->systemInfo.bAllowWriteCombining) {
        /*
         * XXX This call is required only if dumb buffer is going
         * to be presented.
         */
         nv_drm_write_combine_flush();
    }

    if (!nvKms->applyModeSetConfig(nv_dev->pDevice,
                                   requested_config, commit)) {
        return -EINVAL;
    }

    return 0;
}

int nv_drm_atomic_check(struct drm_device *dev,
                        struct drm_atomic_state *state)
{
    int ret = 0;

    if ((ret = drm_atomic_helper_check(dev, state)) != 0) {
        goto done;
    }

    ret = nv_drm_atomic_apply_modeset_config(dev,
                                             state, false /* commit */);

done:
    return ret;
}

/**
 * __nv_drm_handle_flip_event - handle flip occurred event
 * @nv_crtc: crtc on which flip has been occurred
 *
 * This handler dequeues the first nv_drm_flip from the crtc's flip_list,
 * generates an event if requested at flip time, and frees the nv_drm_flip.
 */
static void __nv_drm_handle_flip_event(struct nv_drm_crtc *nv_crtc)
{
    struct drm_device *dev = nv_crtc->base.dev;
    struct nv_drm_device *nv_dev = to_nv_device(dev);
    struct nv_drm_flip *nv_flip;

    /*
     * Acquire event_lock before nv_flip object dequeue, otherwise immediate
     * flip event delivery from nv_drm_atomic_commit() races ahead and
     * messes up with event delivery order.
     */
    spin_lock(&dev->event_lock);
    nv_flip = nv_drm_crtc_dequeue_flip(nv_crtc);
    if (likely(nv_flip != NULL)) {
        struct nv_drm_flip *nv_deferred_flip, *nv_next_deferred_flip;

        if (nv_flip->event != NULL) {
            drm_crtc_send_vblank_event(&nv_crtc->base, nv_flip->event);
        }

        /*
         * Process flips that were deferred until processing of this nv_flip
         * object.
         */
        list_for_each_entry_safe(nv_deferred_flip,
                                 nv_next_deferred_flip,
                                 &nv_flip->deferred_flip_list, list_entry) {

            if (nv_deferred_flip->event != NULL) {
                drm_crtc_send_vblank_event(&nv_crtc->base,
                                           nv_deferred_flip->event);
            }
            list_del(&nv_deferred_flip->list_entry);

            nv_drm_free(nv_deferred_flip);
        }
    }
    spin_unlock(&dev->event_lock);

    wake_up_all(&nv_dev->flip_event_wq);

    nv_drm_free(nv_flip);
}

int nv_drm_atomic_commit(struct drm_device *dev,
                         struct drm_atomic_state *state,
                         bool nonblock)
{
    int ret = -EBUSY;

    int i;
    struct drm_crtc *crtc = NULL;
    struct drm_crtc_state *crtc_state = NULL;
    struct nv_drm_device *nv_dev = to_nv_device(dev);

    /*
     * drm_mode_config_funcs::atomic_commit() mandates to return -EBUSY
     * for nonblocking commit if previous updates (commit tasks/flip event) are
     * pending. In case of blocking commits it mandates to wait for previous
     * updates to complete.
     */
    if (nonblock) {
        nv_drm_for_each_crtc_in_state(state, crtc, crtc_state, i) {
            struct nv_drm_crtc *nv_crtc = to_nv_crtc(crtc);

            /*
             * Here you aren't required to hold nv_drm_crtc::flip_list_lock
             * because:
             *
             * The core DRM driver acquires lock for all affected crtcs before
             * calling into ->commit() hook, therefore it is not possible for
             * other threads to call into ->commit() hook affecting same crtcs
             * and enqueue flip objects into flip_list -
             *
             *   nv_drm_atomic_commit_internal()
             *     |-> nv_drm_atomic_apply_modeset_config(commit=true)
             *           |-> nv_drm_crtc_enqueue_flip()
             *
             * Only possibility is list_empty check races with code path
             * dequeuing flip object -
             *
             *   __nv_drm_handle_flip_event()
             *     |-> nv_drm_crtc_dequeue_flip()
             *
             * But this race condition can't lead list_empty() to return
             * incorrect result. nv_drm_crtc_dequeue_flip() in the middle of
             * updating the list could not trick us into thinking the list is
             * empty when it isn't.
             */
            if (!list_empty(&nv_crtc->flip_list)) {
                return -EBUSY;
            }
        }
    }

#if defined(NV_DRM_ATOMIC_HELPER_SWAP_STATE_HAS_STALL_ARG)

    /*
     * nv_drm_atomic_commit_internal()
     * implements blocking/non-blocking atomic commit using
     * nv_drm_crtc::flip_list, it does not require any help from core DRM
     * helper functions to stall commit processing.  Therefore passing false to
     * 'stall' parameter.
     * In this context, failure from drm_atomic_helper_swap_state() is not
     * expected.
     */

#if defined(NV_DRM_ATOMIC_HELPER_SWAP_STATE_RETURN_INT)
    ret = drm_atomic_helper_swap_state(state, false /* stall */);
    if (WARN_ON(ret != 0)) {
        return ret;
    }
#else
    drm_atomic_helper_swap_state(state, false /* stall */);
#endif

#else
    drm_atomic_helper_swap_state(dev, state);
#endif

    /*
     * nv_drm_atomic_commit_internal() must not return failure after
     * calling drm_atomic_helper_swap_state().
     */

    if ((ret = nv_drm_atomic_apply_modeset_config(
                    dev,
                    state, true /* commit */)) != 0) {
        NV_DRM_DEV_LOG_ERR(
            nv_dev,
            "Failed to apply atomic modeset.  Error code: %d",
            ret);

        goto done;
    }

    nv_drm_for_each_crtc_in_state(state, crtc, crtc_state, i) {
        struct nv_drm_crtc *nv_crtc = to_nv_crtc(crtc);
        struct nv_drm_crtc_state *nv_new_crtc_state =
            to_nv_crtc_state(crtc->state);

        /*
         * If nv_drm_atomic_apply_modeset_config() hasn't consumed the flip
         * object, no event will be generated for this flip, and we need process
         * it:
         */

        if (nv_new_crtc_state->nv_flip != NULL) {
            /*
             * First, defer processing of all pending flips for this crtc until
             * last flip in the queue has been processed. This is to ensure a
             * correct order in event delivery.
             */
            spin_lock(&nv_crtc->flip_list_lock);
            if (!list_empty(&nv_crtc->flip_list)) {
                struct nv_drm_flip *nv_last_flip =
                    list_last_entry(&nv_crtc->flip_list,
                                    struct nv_drm_flip, list_entry);

                list_add(&nv_new_crtc_state->nv_flip->list_entry,
                    &nv_last_flip->deferred_flip_list);

                nv_new_crtc_state->nv_flip = NULL;
            }
            spin_unlock(&nv_crtc->flip_list_lock);
        }

        if (nv_new_crtc_state->nv_flip != NULL) {
            /*
             * Then, if no more pending flips for this crtc, deliver event for the
             * current flip.
             */
            if (nv_new_crtc_state->nv_flip->event != NULL) {
                spin_lock(&dev->event_lock);
                drm_crtc_send_vblank_event(crtc,
                                           nv_new_crtc_state->nv_flip->event);
                spin_unlock(&dev->event_lock);
            }

            nv_drm_free(nv_new_crtc_state->nv_flip);
            nv_new_crtc_state->nv_flip = NULL;
        }

        if (!nonblock) {
            /*
             * Here you aren't required to hold nv_drm_crtc::flip_list_lock
             * because:
             *
             * The core DRM driver acquires lock for all affected crtcs before
             * calling into ->commit() hook, therefore it is not possible for
             * other threads to call into ->commit() hook affecting same crtcs
             * and enqueue flip objects into flip_list -
             *
             *   nv_drm_atomic_commit_internal()
             *     |-> nv_drm_atomic_apply_modeset_config(commit=true)
             *           |-> nv_drm_crtc_enqueue_flip()
             *
             * Only possibility is list_empty check races with code path
             * dequeuing flip object -
             *
             *   __nv_drm_handle_flip_event()
             *     |-> nv_drm_crtc_dequeue_flip()
             *
             * But this race condition can't lead list_empty() to return
             * incorrect result. nv_drm_crtc_dequeue_flip() in the middle of
             * updating the list could not trick us into thinking the list is
             * empty when it isn't.
             */
            if (wait_event_timeout(
                    nv_dev->flip_event_wq,
                    list_empty(&nv_crtc->flip_list),
                    3 * HZ /* 3 second */) == 0) {
                NV_DRM_DEV_LOG_ERR(
                    nv_dev,
                    "Flip event timeout on head %u", nv_crtc->head);
            }
        }
    }

done:

#if defined(NV_DRM_ATOMIC_STATE_REF_COUNTING_PRESENT)
    drm_atomic_state_put(state);
#else
    drm_atomic_state_free(state);
#endif

    return 0;
}

void nv_drm_handle_flip_occurred(struct nv_drm_device *nv_dev,
                                 NvU32 head, NvKmsKapiLayerType plane)
{
    struct nv_drm_crtc *nv_crtc = nv_drm_crtc_lookup(nv_dev, head);

    if (NV_DRM_WARN(nv_crtc == NULL)) {
        return;
    }

    switch (plane) {
        case NVKMS_KAPI_LAYER_PRIMARY:
            __nv_drm_handle_flip_event(nv_crtc);
            break;
        case NVKMS_KAPI_LAYER_OVERLAY:
            /* TODO */
        case NVKMS_KAPI_LAYER_CURSOR:
        default:
            BUG_ON(1);
    }
}

#endif