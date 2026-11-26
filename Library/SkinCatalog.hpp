#pragma once
#include "ChartCatalog.hpp"
#include <optional>

namespace skincatalog {
    // ワーカーはファイルのメタデータだけを読む。DxLibのハンドルには触れない。
    inline catalog::Snapshot Scan(const std::filesystem::path& root) {
        catalog::Snapshot result;
        std::error_code ec;
        if (!std::filesystem::exists(root,ec)) { result.Complete=!ec; return result; }
        auto it=std::filesystem::recursive_directory_iterator(root,ec);
        if(ec){result.Complete=false;return result;}
        for(;it!=std::filesystem::recursive_directory_iterator();it.increment(ec)) {
            if(ec){result.Complete=false;break;}
            if(!it->is_regular_file(ec)){if(ec){result.Complete=false;ec.clear();}continue;}
            const auto size=it->file_size(ec);
            if(ec){result.Complete=false;ec.clear();continue;}
            const auto time=it->last_write_time(ec);
            if(ec){result.Complete=false;ec.clear();continue;}
            result.Entries[catalog::Key(it->path())]={size,(int64_t)time.time_since_epoch().count()};
        }
        if(ec)result.Complete=false;
        return result;
    }
    struct Watch {
        catalog::Snapshot Baseline;
        std::optional<catalog::Snapshot> Candidate;
        bool Started=false;
        bool Accept(catalog::Snapshot snapshot) {
            if(!snapshot.Complete){Candidate.reset();return false;}
            if(!Started){Baseline=std::move(snapshot);Started=true;return false;}
            if(snapshot.Entries==Baseline.Entries){Candidate.reset();return false;}
            if(Candidate&&Candidate->Entries==snapshot.Entries){Baseline=std::move(snapshot);Candidate.reset();return true;}
            Candidate=std::move(snapshot);return false;
        }
    };
}
