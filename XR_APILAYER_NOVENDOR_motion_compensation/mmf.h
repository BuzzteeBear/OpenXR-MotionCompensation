// credit for this code goes to Carlo Milanesi https://github.com/carlomilanesi/cpp-mmf
# pragma once 

namespace memory_mapped_file
{
    unsigned int mmf_granularity();

    class base_mmf
    {
    public:
        explicit base_mmf();
        ~base_mmf();
        size_t offset() const { return offset_; }
        size_t mapped_size() const { return mapped_size_; }
        size_t file_size() const { return file_size_; }
        void unmap();
        void close();
        bool is_open() const
        {
            return file_handle_ != (void*)-1;
        }  
        typedef void* HANDLE;
        HANDLE file_handle() const
        {
            return file_handle_;
        }
    protected:
        size_t query_file_size_();
        char* data_;
        size_t offset_;
        size_t mapped_size_;
        size_t file_size_;
        int granularity_;
        HANDLE file_handle_;
        HANDLE file_mapping_handle_;
    };

    class read_only_mmf: public base_mmf
    {
    public:
        explicit read_only_mmf(char const* pathname = 0, bool map_all = true);
        void open(char const* pathname, bool map_all = true);
        char const* data() const { return data_; }
        void map(size_t offset = 0, size_t size = 0);
    };

    enum mmf_exists_mode
    {
        if_exists_fail,
        if_exists_just_open,
        if_exists_map_all,
        if_exists_truncate,
    };

    enum mmf_doesnt_exist_mode
    {
        if_doesnt_exist_fail,
        if_doesnt_exist_create,
    };

    class writable_mmf: public base_mmf
    {
    public:
        explicit writable_mmf(char const* pathname = 0,
            mmf_exists_mode exists_mode = if_exists_fail,
            mmf_doesnt_exist_mode doesnt_exist_mode = if_doesnt_exist_create);
        void open(char const* pathname,
            mmf_exists_mode exists_mode = if_exists_fail,
            mmf_doesnt_exist_mode doesnt_exist_mode = if_doesnt_exist_create);
        char* data() { return data_; }
        void map(size_t offset = 0, size_t size = 0);
        bool flush();
    };
}
