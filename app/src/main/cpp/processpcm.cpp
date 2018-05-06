#include <jni.h>
#include <android/log.h>
#include <string>
#include <malloc.h>
extern "C"{
#include "include/sox.h"
#include "include/util.h"
}
#define LOG_I(...) __android_log_print(ANDROID_LOG_ERROR , "main", __VA_ARGS__)

typedef struct {
    int16_t *ibuf;
    size_t ilen;
} input_combiner_t;
jsize length;
short *inputData;
short *outputData;
jboolean *args;

static int combiner_start(sox_effect_t *effp) {
    LOG_I("combiner_start");
    input_combiner_t *z = (input_combiner_t *) effp->priv;
    z->ibuf = (int16_t *) lsx_malloc(sizeof(int16_t) * 8192);
    return SOX_SUCCESS;
}
//input
static int combiner_drain(sox_effect_t *effp, sox_sample_t *obuf, size_t *osamp) {
    LOG_I("combiner_drain");
    input_combiner_t *z = (input_combiner_t *) effp->priv;
    size_t nr = *osamp > 8192 ? 8192 : *osamp;
    memcpy(z->ibuf,inputData, nr);
    size_t olen = *osamp;//fread(z->ibuf, sizeof(int16_t), nr, z->fp);
    LOG_I("osmap length=%d",olen);//8192
    for (int i = 0; i < olen; ++i) {
        obuf[i] = SOX_SIGNED_16BIT_TO_SAMPLE(z->ibuf[i], 0);
    }
    *osamp = olen;
    return olen ? SOX_SUCCESS : SOX_EOF;
}

static int combiner_stop(sox_effect_t *effp) {
    LOG_I("combiner_stop");
    input_combiner_t *z = (input_combiner_t *) effp->priv;
    free(z->ibuf);
    return SOX_SUCCESS;
}

static sox_effect_handler_t const *input_combiner_effect_fn(void) {
    LOG_I("input_combiner_effect_fn");
    static sox_effect_handler_t handler = {"input", 0, SOX_EFF_MCHAN |
                                                       SOX_EFF_MODIFY, 0, combiner_start, 0,
                                           combiner_drain,
                                           combiner_stop, 0, sizeof(input_combiner_t)
    };
    return &handler;
}

typedef struct {
    int16_t *ibuf;
} output_combiner_t;

static int ostart(sox_effect_t *effp) {
    LOG_I("ostart");
    output_combiner_t *z = (output_combiner_t *) effp->priv;
    z->ibuf = (int16_t *) lsx_malloc(sizeof(int16_t) * 8192);
    unsigned prec = effp->out_signal.precision;
    if (effp->in_signal.mult && effp->in_signal.precision > prec)
        *effp->in_signal.mult *= 1 - (1 << (31 - prec)) * (1. / SOX_SAMPLE_MAX);
    return SOX_SUCCESS;
}
//output
static int output_flow(sox_effect_t *effp, sox_sample_t const *ibuf,
                       sox_sample_t *obuf, size_t *isamp, size_t *osamp) {
    LOG_I("output_flow");
    output_combiner_t *z = (output_combiner_t *) effp->priv;
    size_t olen = *isamp;
    LOG_I("olen = %d",olen);//1120 //0 7840 6176
    if (olen > 8192) {
        LOG_I("olen > length");
        exit(-1);
    }
    SOX_SAMPLE_LOCALS;
    int flip = 0;
    for (int i = 0; i < olen; ++i) {
        z->ibuf[i] = SOX_SAMPLE_TO_SIGNED_16BIT(ibuf[i], flip);
    }
    memcpy(outputData,z->ibuf,*isamp);
    *osamp = 0;
    return SOX_SUCCESS;
}

static int output_stop(sox_effect_t *effp) {
    LOG_I("output_stop");
    output_combiner_t *z = (output_combiner_t *) effp->priv;
    free(z->ibuf);
    return SOX_SUCCESS;
}

static sox_effect_handler_t const *output_effect_fn(void) {
    LOG_I("output_effect_fn");
    static sox_effect_handler_t handler = {"output", 0, SOX_EFF_MCHAN |
                                                        SOX_EFF_MODIFY | SOX_EFF_PREC, NULL, ostart,
                                           output_flow, NULL, output_stop, NULL,
                                           sizeof(output_combiner_t)
    };
    return &handler;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_gx_ffmpegplayer_MainActivity_initSox2(JNIEnv *env, jobject instance) {
    int ret = sox_init();
    if (ret != SOX_SUCCESS){
        LOG_I("sox_init failed");
        sox_quit();
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_gx_ffmpegplayer_MainActivity_closeSox2(JNIEnv *env, jobject instance) {
    sox_quit();
}

extern "C"
JNIEXPORT jshortArray JNICALL
Java_com_example_gx_ffmpegplayer_MainActivity_pcmbuffer(JNIEnv *env, jobject instance, jshortArray bytes_, jbooleanArray args_) {
    inputData = env->GetShortArrayElements(bytes_, NULL);
    args = env->GetBooleanArrayElements(args_, NULL);
    length = env->GetArrayLength(bytes_);
    LOG_I("length=%d",length);
    outputData = (short *) malloc(length);

    sox_encodinginfo_t in_enc;
    sox_signalinfo_t in_sig;

    in_enc.bits_per_sample = 16;
    in_enc.encoding = SOX_ENCODING_SIGN2;
    in_enc.compression = 0.f;
    in_enc.opposite_endian = sox_false;
    in_enc.reverse_bits = sox_option_no;
    in_enc.reverse_bytes = sox_option_no;
    in_enc.reverse_nibbles = sox_option_no;

    in_sig.rate = 44100.f;
    in_sig.channels = 1;
    in_sig.length = 0;
    in_sig.precision = 16;
    in_sig.mult = NULL;
    sox_encodinginfo_t out_enc = in_enc;
    sox_signalinfo_t out_sig = in_sig;
    sox_effects_chain_t *chain = sox_create_effects_chain(&in_enc, &out_enc);
    sox_effect_t *effp;
    if (chain->length == 0) {
        effp = sox_create_effect(input_combiner_effect_fn());
        sox_add_effect(chain, effp, &in_sig, &out_sig);
        free(effp);
    }
    effp = sox_create_effect(sox_find_effect("pitch"));
    char pp[32] = "100";
    char *const arg[1] = {pp};
    int ret = sox_effect_options(effp, 1, arg);
    sox_add_effect(chain, effp, &in_sig, &out_sig);
    free(effp);
    effp = sox_create_effect(output_effect_fn());
    if (sox_add_effect(chain, effp, &in_sig, &out_sig) != SOX_SUCCESS)
        LOG_I("sox_add_effect error");
    free(effp);
    int flow_status = sox_flow_effects(chain, NULL, NULL);
    sox_delete_effects_chain(chain);
    env->ReleaseShortArrayElements(bytes_, inputData, 0);
    env->ReleaseBooleanArrayElements(args_, args, 0);
    jshortArray jshortArray1 = env->NewShortArray(length);
    env->SetShortArrayRegion(jshortArray1, 0, length, outputData);
    return jshortArray1;
}

