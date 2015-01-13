#ifndef SIGNATURE_STATE_H
#define SIGNATURE_STATE_H

/**
 * SignatureStates are stored into the database
 * Ready: the most recent version of the file is already signed;
 * Not_ready: no versions of the file is ready;
 * Ready_old: the most recent version of the file is not ready, while an older version is.
 */
enum SignatureState {READY, NOT_READY, READY_OLD};

#endif