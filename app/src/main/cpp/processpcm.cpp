#include <jni.h>
#include <android/log.h>
#include <string>
#include <malloc.h>
extern "C"{
#include "include/sox.h"
#include "include/util.h"
}
#define LOG_I(...) __android_log_print(ANDROID_LOG_ERROR , "main", __VA_ARGS__)

jsize length;
short *inputData;
short *outputData;
jboolean *args;

typedef struct {
    int16_t *ibuf;
    size_t ilen;
} input_combiner_t;

static int combiner_start(sox_effect_t *effp) {
    //LOG_I("combiner_start");
    input_combiner_t *z = (input_combiner_t *) effp->priv;
    z->ibuf = (int16_t *) lsx_malloc(sizeof(int16_t) * 8192);
    return SOX_SUCCESS;
}

static int combiner_drain(sox_effect_t *effp, sox_sample_t *obuf, size_t *osamp) {
    //LOG_I("combiner_drain");
    input_combiner_t *z = (input_combiner_t *) effp->priv;
    size_t nr = *osamp > 8192 ? 8192 : *osamp;
    memcpy(z->ibuf,inputData, sizeof(short)*length);
    //LOG_I("read length=%d,sizeof input=%d",nr,sizeof(short)*length);//8192
    for (int i = 0; i < sizeof(short)*length; ++i) {
        obuf[i] = SOX_SIGNED_16BIT_TO_SAMPLE(z->ibuf[i], 0);
    }
    *osamp = nr;
    return nr ? SOX_SUCCESS : SOX_EOF;
}

static int combiner_stop(sox_effect_t *effp) {
    input_combiner_t *z = (input_combiner_t *) effp->priv;
    free(z->ibuf);
    return SOX_SUCCESS;
}
static sox_effect_handler_t const *input_combiner_effect_fn(void) {
    //LOG_I("input_combiner_effect_fn");
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
    output_combiner_t *z = (output_combiner_t *) effp->priv;
    z->ibuf = (int16_t *) lsx_malloc(sizeof(int16_t) * 8192);
    unsigned prec = effp->out_signal.precision;
    if (effp->in_signal.mult && effp->in_signal.precision > prec)
        *effp->in_signal.mult *= 1 - (1 << (31 - prec)) * (1. / SOX_SAMPLE_MAX);
    return SOX_SUCCESS;
}

static int output_flow(sox_effect_t *effp, sox_sample_t const *ibuf,
                       sox_sample_t *obuf, size_t *isamp, size_t *osamp) {
    output_combiner_t *z = (output_combiner_t *) effp->priv;
    size_t olen = *isamp;
    //LOG_I("olen = %d",olen);
    if (olen > 8192) {
        exit(-1);
    }
    SOX_SAMPLE_LOCALS;
    int flip = 0;
    for (int i = 0; i < olen; ++i) {
        z->ibuf[i] = SOX_SAMPLE_TO_SIGNED_16BIT(ibuf[i], flip);
    }

    //LOG_I("size =%d",*isamp);
    memcpy(outputData,z->ibuf,*isamp);
//    fwrite(z->ibuf, sizeof(int16_t), *isamp, z->fp);
    *osamp = 0;
    return SOX_SUCCESS;
}
static int output_stop(sox_effect_t *effp) {
    output_combiner_t *z = (output_combiner_t *) effp->priv;
    free(z->ibuf);
    return SOX_SUCCESS;
}

static sox_effect_handler_t const *output_effect_fn(void) {
    //LOG_I("output_effect_fn");
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
        //LOG_I("sox_init failed");
        sox_quit();
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_gx_ffmpegplayer_MainActivity_closeSox2(JNIEnv *env, jobject instance) {
    sox_quit();
}

int drain_cnt_down = 1;
int flow_frequency = 0;
static sox_format_t * in, * out;

void addEffect(sox_effects_chain_t *chain, sox_effect_t *effp, int ret, sox_signalinfo_t *in_sig,
               sox_signalinfo_t *out_sig);

static int reverb_input_drain(sox_effect_t * effp, sox_sample_t * obuf, size_t * osamp) {
    //LOG_I("input_drain success...");
//    if(drain_cnt_down > 0){
//        drain_cnt_down = 0;
//        *osamp = length;
//        int i = 0;
//        for (; i < *osamp; i++) {
//            obuf[i] = SOX_SIGNED_16BIT_TO_SAMPLE(inputData[i], 0);//((sox_sample_t) (inputData[i]) << 16);
//        }
//        //LOG_I("input_drain ... *osamp is %d return SOX_SUCCESS", *osamp);
//        return SOX_SUCCESS;
//    } else{
//        //LOG_I("input_drain once execute must return SOX_EOF");
//        *osamp = 0;
//        return SOX_EOF;
//    }
    (void)effp;
    *osamp -= *osamp % effp->out_signal.channels;
    *osamp = sox_read(in, obuf, *osamp);
    return *osamp? SOX_SUCCESS : SOX_EOF;
}

static int reverb_output_flow(sox_effect_t *effp LSX_UNUSED, sox_sample_t const * ibuf, sox_sample_t * obuf LSX_UNUSED, size_t * isamp, size_t * osamp) {
    LOG_I("reverb_output_flow output_flow... *isamp is %d *osamp is %d", *isamp, *osamp);
//    if (*isamp) {
//        int i = 0;
//        SOX_SAMPLE_LOCALS;
//        int flip=0;
//        //LOG_I("flow_frequency=%d",flow_frequency);
//        for (; i < *isamp; i++) {
//            outputData[i + (*isamp * flow_frequency)] = SOX_SAMPLE_TO_SIGNED_16BIT(ibuf[i], flip);//(short) (ibuf[i] >> 16);
//        }
//        flow_frequency++;
//    }
//    *osamp = 0;
//    //LOG_I("output_flow success...");
//    return SOX_SUCCESS;

//    if (*isamp==0){
//        *osamp = 0;
//        return SOX_SUCCESS;
//    }
    size_t len = sox_write(out, ibuf, *isamp);
    if (len != *isamp) {
        LOG_I("len=%d,ismap=%d",len,*isamp);
        return SOX_EOF;
    }
    *osamp = 0;
    (void)effp;
    return SOX_SUCCESS;
}

static sox_effect_handler_t const * reverb_input_handler(void) {
    static sox_effect_handler_t handler = { "input", NULL, SOX_EFF_MCHAN | SOX_EFF_MODIFY, NULL, NULL, NULL, reverb_input_drain, NULL, NULL, 0 };
    return &handler;
}

static sox_effect_handler_t const * reverb_output_handler(void) {
    static sox_effect_handler_t handler = { "output", NULL, SOX_EFF_MCHAN | SOX_EFF_MODIFY | SOX_EFF_PREC, NULL, NULL, reverb_output_flow, NULL, NULL, NULL, 0 };
    return &handler;
}

extern "C"
JNIEXPORT jshortArray JNICALL
Java_com_example_gx_ffmpegplayer_MainActivity_pcmbuffer(JNIEnv *env, jobject instance, jshortArray bytes_, jbooleanArray args_) {
    inputData = env->GetShortArrayElements(bytes_, NULL);
    args = env->GetBooleanArrayElements(args_, NULL);
    length = env->GetArrayLength(bytes_);
    outputData = (short *) malloc(sizeof(short)*length);
    drain_cnt_down = 1;
    flow_frequency = 0;
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
    // set sane SoX global default values
    struct sox_globals_t* sox_globals_p = sox_get_globals ();
    sox_globals_p->bufsiz = 2048;

    in = sox_open_mem_read(inputData, length*2, NULL, NULL, "s16");
    if (!in){
        LOG_I("sox_open_mem_read fail");
    }
    out = sox_open_mem_write(outputData, length*2, &in_sig, NULL, "s16", NULL);
    if (!out){
        LOG_I("sox_open_mem_write fail");
    }

    sox_effects_chain_t *chain = sox_create_effects_chain(&in_enc, &out_enc);
    sox_effect_t *effp;
    if (chain->length == 0) {
        effp = sox_create_effect(reverb_input_handler());
        sox_add_effect(chain, effp, &in_sig, &in_sig);
        free(effp);
    }
    int ret = 0;
    //添加声音音效
    effp = sox_create_effect(sox_find_effect("vol"));
    char* volargs[1];
    volargs[0] = "-3dB";
    ret = sox_effect_options(effp, 1,  volargs);
    if(ret != SOX_SUCCESS){
        LOG_I("sox_effect_options fail");
    }
    ret = sox_add_effect(chain, effp, &in_sig, &in_sig);
    if (ret != SOX_SUCCESS){
        LOG_I("sox_add_effect fail");
    }
    free(effp);
    //添加其他音效
    addEffect(chain, effp, ret, &in_sig, &out_sig);
    //输出
    effp = sox_create_effect(reverb_output_handler());
    ret = sox_add_effect(chain, effp, &in_sig, &in_sig);
    if (ret != SOX_SUCCESS){
        LOG_I("sox_add_effect fail");
    }
    ret = sox_flow_effects(chain, NULL, NULL);
    if (ret != SOX_SUCCESS){
        LOG_I("sox_flow_effects fail %d",ret);//End Of File or other error
    }
    sox_delete_effects_chain(chain);
    env->ReleaseShortArrayElements(bytes_, inputData, 0);
    env->ReleaseBooleanArrayElements(args_, args, 0);
    jshortArray jshortArray1 = env->NewShortArray(length);
    env->SetShortArrayRegion(jshortArray1, 0, length, outputData);
    free(outputData);
    return jshortArray1;
}

void addEffect(sox_effects_chain_t *chain, sox_effect_t *effp, int ret, sox_signalinfo_t *in_sig,
               sox_signalinfo_t *out_sig) {
    effp = sox_create_effect(sox_find_effect("pitch"));
    char pp[32] = "10";
    char *const arg[1] = {pp};
    ret = sox_effect_options(effp, 1, arg);
    if (ret!=SOX_SUCCESS){
        LOG_I("sox_effect_options error");
    }
    ret = sox_add_effect(chain, effp, in_sig, in_sig);
    if (ret!=SOX_SUCCESS){
        LOG_I("sox_add_effect error");
    }
    free(effp);
    effp = sox_create_effect(sox_find_effect("rate")); /* Should always succeed. */
    ret = sox_effect_options(effp, 0, NULL);
    if (ret!=SOX_SUCCESS){
        LOG_I("sox_effect_options error");
    }
        //LOG_I("rate EOF"); /* The failing effect should have displayed an error message */
    ret = sox_add_effect(chain, effp, in_sig, in_sig);
    if (ret!=SOX_SUCCESS){
        LOG_I("sox_add_effect error");
    }
    free(effp);
    effp = sox_create_effect(sox_find_effect("dither")); /* Should always succeed. */
    char dither[3] =  "-S";
    char *const ditargs[1] = {dither};
    ret = sox_effect_options(effp, 0, NULL);
    if (ret!=SOX_SUCCESS){
        LOG_I("sox_effect_options error");
    }
    //LOG_I("dither EOF"); /* The failing effect should have displayed an error message */
    sox_add_effect(chain, effp, in_sig, in_sig);
    free(effp);
    //compand
    effp = sox_create_effect(sox_find_effect("compand"));
    char* attackRelease = "0.3,1.0";
    char* functionTransTable = "6:-90,-90,-70,-55,-31,-21,0,-20";
    char* gain = "0";
    char* initialVolume = "-90";
    char* delay = "0.1";
    char* compandArgs[] = {attackRelease,functionTransTable,gain,initialVolume,delay};
    sox_effect_options(effp,5,compandArgs);
    if (ret!=SOX_SUCCESS){
        LOG_I("sox_effect_options error");
    }
    ret = sox_add_effect(chain, effp, in_sig, in_sig);
    if (ret!=SOX_SUCCESS){
        LOG_I("sox_add_effect error");
    }
    free(effp);
    //reverb
//    effp = sox_create_effect(sox_find_effect("reverb"));
//    char* wetOnly = "-w";
//    char* reverbrance = "50";
//    char* hfDamping = "50";
//    char* roomScale = "100";
//    char* stereoDepth = "100";
//    char* preDelay = "0";
//    char* wetGain = "0";
//    char* reverbArgs[] = {wetOnly,reverbrance,hfDamping,roomScale,stereoDepth,preDelay,wetGain};
//    ret = sox_effect_options(effp,7,reverbArgs);
//    sox_add_effect(chain, effp, in_sig, out_sig);
//    free(effp);
    //echo
    effp = sox_create_effect(sox_find_effect("echo"));
    char* arg1 = "0.8";
    char* arg2 = "0.9";
    char* arg3 = "1000";
    char* arg4 = "0.3";
    char* arg5 = "1800";
    char* arg6 = "0.25";
    char* echoArgs[] = {arg1,arg2,arg3,arg4,arg5,arg6};
    ret = sox_effect_options(effp, 6, echoArgs);
    if (ret!=SOX_SUCCESS){
        //LOG_I("sox_effect_options error");
    }
    ret = sox_add_effect(chain, effp, in_sig, in_sig);
    if (ret!=SOX_SUCCESS){
        //LOG_I("sox_add_effect error");
    }
    free(effp);
}

