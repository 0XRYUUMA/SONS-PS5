#include <cstdint>
#include "prx/common/StderrLog.hpp"
#include <cstdio>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libSceSaveDataDialog.native/SaveDataDialog.hpp"
#include "prx/libc/include/General.hpp"

namespace {

int SoftFailNp(const char* func) {
    aps5::LogErr( "[SOFT-NP] %s -> 0 (unimplemented SCE API - no-op)\n", func);
    aps5::LogFlush(aps5::LogStdErr);
    return 0;
}
}

extern "C" {

 int APS5_VABI sceSaveDataDialogClose(const void* close_param) {
  (void)close_param;
  return SoftFailNp(__func__);
 }

 int APS5_VABI sceSaveDataDialogGetResult(void* result) {

  if (result == nullptr) {
   return SAVE_DATA_DIALOG_ERROR_ARG_NULL;
  }
  auto* r = static_cast<SaveDataDialogResult*>(result);
  std::memset(r, 0, sizeof(*r));
  r->result = SAVE_DATA_DIALOG_RESULT_OK;
  r->button_id = SAVE_DATA_DIALOG_BUTTON_ID_OK;
  return SoftFailNp(__func__);
 }

 int APS5_VABI sceSaveDataDialogInitialize(void) {
  return SoftFailNp(__func__);
 }

 int APS5_VABI sceSaveDataDialogIsReadyToDisplay(void) {
  return SoftFailNp(__func__);
 }

 int APS5_VABI sceSaveDataDialogOpen(const void* param) {
  (void)param;
  return SoftFailNp(__func__);
 }

 int APS5_VABI sceSaveDataDialogProgressBarInc(int target, uint32_t delta) {
  (void)target;
  (void)delta;
  return SoftFailNp(__func__);
 }

 int APS5_VABI sceSaveDataDialogProgressBarSetValue(int target, uint32_t rate) {
  (void)target;
  (void)rate;
  return SoftFailNp(__func__);
 }

 int APS5_VABI sceSaveDataDialogTerminate(void) {
  return SoftFailNp(__func__);
 }

}
