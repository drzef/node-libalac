// ALAC.node, © 2014 Stéphan Kochen
// MIT-licensed. (See the included LICENSE file.)

#include <v8.h>
#include <node.h>
#include <node_buffer.h>
#include <ALACBitUtilities.h>
#include <ALACEncoder.h>
#include <ALACDecoder.h>

using namespace v8;
using namespace node;

namespace alac {


static Persistent<String> frames_per_packet_symbol;
static Persistent<String> sample_rate_symbol;
static Persistent<String> channels_symbol;
static Persistent<String> bit_depth_symbol;


// XXX: Stolen from convert-utility.
enum
{
    kTestFormatFlag_16BitSourceData    = 1,
    kTestFormatFlag_20BitSourceData    = 2,
    kTestFormatFlag_24BitSourceData    = 3,
    kTestFormatFlag_32BitSourceData    = 4
};

static Handle<Value>
throw_alac_error(int32_t ret)
{
  // XXX: The remaining values are unused in the codec itself.
  switch (ret) {
    case kALAC_ParamError:
      return ThrowException(Exception::Error(
          String::New("ALAC error: invalid parameter")));
    case kALAC_MemFullError:
      return ThrowException(Exception::Error(
          String::New("ALAC error: out of memory")));
    default:
      return ThrowException(Exception::Error(
          String::New("ALAC error: unknown error")));
  }
}

static void
init_pcm_format(Handle<Object> o, AudioFormatDescription &f)
{
  Handle<Value> v;

  f.mFormatID = kALACFormatLinearPCM;
  f.mFormatFlags = 0;  // XXX: These are pretty much ignored.
  f.mFramesPerPacket = 1;
  f.mReserved = 0;

  f.mSampleRate = o->Get(sample_rate_symbol)->NumberValue();
  f.mChannelsPerFrame = o->Get(channels_symbol)->Uint32Value();
  f.mBitsPerChannel = o->Get(bit_depth_symbol)->Uint32Value();

  f.mBytesPerPacket = f.mBytesPerFrame = f.mChannelsPerFrame * (f.mBitsPerChannel >> 3);
}

static void
init_alac_format(Handle<Object> o, AudioFormatDescription &f)
{
  Handle<Value> v;

  f.mFormatID = kALACFormatAppleLossless;
  f.mReserved = 0;

  // XXX: Zero because it's VBR.
  f.mBytesPerPacket = 0;
  f.mBytesPerFrame = 0;
  f.mBitsPerChannel = 0;

  f.mFramesPerPacket = o->Get(frames_per_packet_symbol)->Uint32Value();
  f.mSampleRate = o->Get(sample_rate_symbol)->NumberValue();
  f.mChannelsPerFrame = o->Get(channels_symbol)->Uint32Value();
  switch (o->Get(bit_depth_symbol)->Uint32Value()) {
    case 20: f.mFormatFlags = kTestFormatFlag_20BitSourceData; break;
    case 24: f.mFormatFlags = kTestFormatFlag_24BitSourceData; break;
    case 32: f.mFormatFlags = kTestFormatFlag_32BitSourceData; break;
    default: f.mFormatFlags = kTestFormatFlag_16BitSourceData; break;
  }
}


class Encoder : ObjectWrap
{
public:
  static void
  Initialize(Handle<Object> target)
  {
    Handle<FunctionTemplate> t = FunctionTemplate::New(New);
    t->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(t, "encode", Encode);
    // XXX: Finish is a no-op, so don't bother.

    target->Set(String::NewSymbol("Encoder"), t->GetFunction());
  }

  virtual
  ~Encoder() {}

private:
  Encoder() : enc_() {}

  static Handle<Value>
  New(const Arguments &args)
  {
    HandleScope scope;

    Handle<Object> o = Handle<Object>::Cast(args[0]);
    Handle<Value> v_ret;

    Encoder *e = new Encoder();

    init_pcm_format(o, e->inf_);
    init_alac_format(o, e->outf_);

    e->enc_.SetFrameSize(e->outf_.mFramesPerPacket);
    int32_t ret = e->enc_.InitializeEncoder(e->outf_);
    if (ret != ALAC_noErr) {
      delete e;
      return throw_alac_error(ret);
    }

    e->Wrap(args.This());
    return scope.Close(e->handle_);
  }

  static Handle<Value>
  Encode(const Arguments &args)
  {
    HandleScope scope;

    Encoder *e = ObjectWrap::Unwrap<Encoder>(args.This());

    unsigned char *in = (unsigned char *) Buffer::Data(args[0]);
    unsigned char *out = (unsigned char *) Buffer::Data(args[1]);
    int32_t size = (int32_t) Buffer::Length(args[0]);

    int32_t ret = e->enc_.Encode(e->inf_, e->outf_, in, out, &size);
    if (ret != ALAC_noErr)
      return throw_alac_error(ret);

    return scope.Close(Uint32::New(size));
  }

private:
  ALACEncoder enc_;
  AudioFormatDescription inf_;
  AudioFormatDescription outf_;
};


static void
Initialize(Handle<Object> target)
{
  HandleScope scope;

  frames_per_packet_symbol = NODE_PSYMBOL("framesPerPacket");
  sample_rate_symbol = NODE_PSYMBOL("sampleRate");
  channels_symbol = NODE_PSYMBOL("channels");
  bit_depth_symbol = NODE_PSYMBOL("bitDepth");

  NODE_DEFINE_CONSTANT(target, kALACDefaultFramesPerPacket);
  NODE_DEFINE_CONSTANT(target, kALACMaxEscapeHeaderBytes);

  Encoder::Initialize(target);
}


} // namespace alac

extern "C" void
init(Handle<Object> target)
{
  alac::Initialize(target);
}

NODE_MODULE(binding, init)
