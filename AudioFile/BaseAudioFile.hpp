#ifndef _HISSTOOLS_BASEAUDIOFILE_
#define _HISSTOOLS_BASEAUDIOFILE_

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace HISSTools
{
    class BaseAudioFile
    {
    public:
        
        enum class FileType     { None, AIFF, AIFC, WAVE };
        enum class PCMFormat    { Int8, Int16, Int24, Int32, Float32, Float64 };
        enum class Endianness   { Little, Big };
        enum class NumericType  { Integer, Float };
        
        enum class Error
        {
            None                    = 0,
            CouldNotAllocate        = 1 << 0,
            FileError               = 1 << 1,
            CouldNotOpen            = 1 << 2,
            BadFormat               = 1 << 3,
            UnknownFormat           = 1 << 4,
            UnsupportedPCMFormat    = 1 << 5,
            WrongAIFCVersion        = 1 << 6,
            UnsupportedAIFCFormat   = 1 << 7,
            UnsupportedWaveFormat   = 1 << 8,
            CouldNotWrite           = 1 << 9,
        };
        
        enum AiffVersion
        {
            AIFC_CURRENT_SPECIFICATION = 0xA2805140
        };
        
        BaseAudioFile()
        : mFileType(FileType::None)
        , mPCMFormat(PCMFormat::Int8)
        , mHeaderEndianness(Endianness::Little)
        , mAudioEndianness(Endianness::Little)
        , mSamplingRate(0)
        , mNumChannels(0)
        , mNumFrames(0)
        , mPCMOffset(0)
        , mErrorFlags(static_cast<int>(Error::None))
        {}
        
        virtual ~BaseAudioFile() {}
        
        bool isOpen() const                     { return mFile.is_open(); }

        void close()
        {
            mFile.close();
            
            mFileType = FileType::None;
            mPCMFormat = PCMFormat::Int8;
            mHeaderEndianness = Endianness::Little;
            mAudioEndianness = Endianness::Little;
            mSamplingRate = 0;
            mNumChannels = 0;
            mNumFrames = 0;
            mPCMOffset = 0;
            mErrorFlags = static_cast<int>(Error::None);
        }
        
        FileType getFileType() const            { return mFileType; }
        PCMFormat getPCMFormat() const          { return mPCMFormat; }
        Endianness getHeaderEndianness() const  { return mHeaderEndianness; }
        Endianness getAudioEndianness() const   { return mAudioEndianness; }
        double getSamplingRate() const          { return mSamplingRate; }
        uint16_t getChannels() const            { return mNumChannels; }
        uintptr_t getFrames() const             { return mNumFrames; }
        uint16_t getBitDepth() const            { return findBitDepth(getPCMFormat()); }
        uint16_t getByteDepth() const           { return getBitDepth() / 8; }
        uintptr_t getFrameByteCount() const     { return getChannels() * getByteDepth(); }
        NumericType getNumericType() const      { return findNumericType(getPCMFormat()); }
        
        bool isError() const                    { return mErrorFlags != static_cast<int>(Error::None); }
        int getErrorFlags() const               { return mErrorFlags; }
        void clearErrorFlags()                  { mErrorFlags = static_cast<int>(Error::None); }
        
        static std::string getErrorString(Error error)
        {
            switch (error)
            {
                case Error::CouldNotAllocate:           return "could not allocate memory";
                case Error::FileError:                  return "file error";
                case Error::CouldNotOpen:               return "couldn't open file";
                case Error::BadFormat:                  return "bad format";
                case Error::UnknownFormat:              return "unknown format";
                case Error::UnsupportedPCMFormat:       return "unsupported pcm format";
                case Error::WrongAIFCVersion:           return "wrong aifc version";
                case Error::UnsupportedAIFCFormat:      return "unsupported aifc format";
                case Error::UnsupportedWaveFormat:      return "unsupported wave format";
                case Error::CouldNotWrite:              return "couldn't write file";
                    
                default:                                return "no error";
            }
        }
        
        static std::vector<Error> extractErrorsFromFlags(int flags)
        {
            std::vector<Error> errors;
            
            for (int i = 0; i <= static_cast<int>(Error::CouldNotWrite); i++)
            {
                if (flags & (1 << i))
                    errors.push_back(static_cast<Error>(i));
            }
            
            return errors;
        }
        
        std::vector<Error> getErrors() const    { return extractErrorsFromFlags(getErrorFlags()); }
        
        static uint16_t findBitDepth(PCMFormat format)
        {
            switch (format)
            {
                case PCMFormat::Int8:       return 8;
                case PCMFormat::Int16:      return 16;
                case PCMFormat::Int24:      return 24;
                case PCMFormat::Int32:      return 32;
                case PCMFormat::Float32:    return 32;
                case PCMFormat::Float64:    return 64;
                    
                default:                    return 16;
            }
        }
        
        static NumericType findNumericType(PCMFormat format)
        {
            switch (format)
            {
                case PCMFormat::Int8:
                case PCMFormat::Int16:
                case PCMFormat::Int24:
                case PCMFormat::Int32:
                    return NumericType::Integer;
                    
                case PCMFormat::Float32:
                case PCMFormat::Float64:
                    return NumericType::Float;
                    
                default:
                    return NumericType::Integer;
            }
        }
        
        template <int N, int M, Endianness E>
        static constexpr int byteShift() { return E == Endianness::Big ? (N - (M + 1)) * 8 : M * 8; }

    protected:
        
        static constexpr uintptr_t WORK_LOOP_SIZE = 1024;
        
        uintptr_t getPCMOffset() const          { return mPCMOffset; }
        
        void setErrorFlags(int flags)           { mErrorFlags = flags; }
        void setErrorBit(Error error)           { mErrorFlags |= static_cast<int>(error); }
        
        template <typename T>
        static T paddedLength(T length)
        {
            return length + (length & 0x1);
        }
        
        FileType mFileType;
        PCMFormat mPCMFormat;
        Endianness mHeaderEndianness;
        Endianness mAudioEndianness;
        
        double mSamplingRate;
        uint16_t mNumChannels;
        uintptr_t mNumFrames;
        size_t mPCMOffset;
        
        //  Data
        
        std::fstream mFile;
        std::vector<unsigned char> mBuffer;
        
    private:
        
        int mErrorFlags;
    };
}

#endif
