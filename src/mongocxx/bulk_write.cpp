// Copyright 2014 MongoDB Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <mongocxx/bulk_write.hpp>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/stdx/make_unique.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <mongocxx/exception/private/mongoc_error.hh>
#include <mongocxx/private/bulk_write.hh>
#include <mongocxx/private/collection.hh>
#include <mongocxx/private/libbson.hh>
#include <mongocxx/private/libmongoc.hh>
#include <mongocxx/private/write_concern.hh>

#include <mongocxx/config/private/prelude.hh>

namespace mongocxx {
MONGOCXX_INLINE_NAMESPACE_BEGIN

using namespace libbson;
using bsoncxx::builder::basic::kvp;

bulk_write::bulk_write(bulk_write&&) noexcept = default;
bulk_write& bulk_write::operator=(bulk_write&&) noexcept = default;

bulk_write::~bulk_write() = default;

bulk_write::bulk_write(options::bulk_write options)
    : _created_from_collection(false),
      _impl(stdx::make_unique<impl>(libmongoc::bulk_operation_new(options.ordered()))) {
    auto options_wc = options.write_concern();
    if (options_wc)
        libmongoc::bulk_operation_set_write_concern(_impl->operation_t,
                                                    options_wc->_impl->write_concern_t);

    auto options_bdv = options.bypass_document_validation();
    if (options_bdv)
        libmongoc::bulk_operation_set_bypass_document_validation(_impl->operation_t, *options_bdv);
}

void bulk_write::append(const model::write& operation) {
    switch (operation.type()) {
        case write_type::k_insert_one: {
            scoped_bson_t doc(operation.get_insert_one().document());

            libmongoc::bulk_operation_insert(_impl->operation_t, doc.bson());
            break;
        }
        case write_type::k_update_one: {
            scoped_bson_t filter(operation.get_update_one().filter());
            scoped_bson_t update(operation.get_update_one().update());

            bsoncxx::builder::basic::document options_builder;
            if (operation.get_update_one().collation()) {
                options_builder.append(kvp("collation", *operation.get_update_one().collation()));
            }
            if (operation.get_update_one().upsert()) {
                options_builder.append(kvp("upsert", *operation.get_update_one().upsert()));
            }
            scoped_bson_t options(options_builder.extract());

            bson_error_t error;
            auto result = libmongoc::bulk_operation_update_one_with_opts(
                _impl->operation_t, filter.bson(), update.bson(), options.bson(), &error);
            if (!result) {
                throw_exception<logic_error>(error);
            }
            break;
        }
        case write_type::k_update_many: {
            scoped_bson_t filter(operation.get_update_many().filter());
            scoped_bson_t update(operation.get_update_many().update());

            bsoncxx::builder::basic::document options_builder;
            if (operation.get_update_many().collation()) {
                options_builder.append(kvp("collation", *operation.get_update_many().collation()));
            }
            if (operation.get_update_many().upsert()) {
                options_builder.append(kvp("upsert", *operation.get_update_many().upsert()));
            }
            scoped_bson_t options(options_builder.extract());

            bson_error_t error;
            auto result = libmongoc::bulk_operation_update_many_with_opts(
                _impl->operation_t, filter.bson(), update.bson(), options.bson(), &error);
            if (!result) {
                throw_exception<logic_error>(error);
            }
            break;
        }
        case write_type::k_delete_one: {
            scoped_bson_t filter(operation.get_delete_one().filter());

            bsoncxx::builder::basic::document options_builder;
            if (operation.get_delete_one().collation()) {
                options_builder.append(kvp("collation", *operation.get_delete_one().collation()));
            }
            scoped_bson_t options(options_builder.extract());

            bson_error_t error;
            auto result = libmongoc::bulk_operation_remove_one_with_opts(
                _impl->operation_t, filter.bson(), options.bson(), &error);
            if (!result) {
                throw_exception<logic_error>(error);
            }
            break;
        }
        case write_type::k_delete_many: {
            scoped_bson_t filter(operation.get_delete_many().filter());

            bsoncxx::builder::basic::document options_builder;
            if (operation.get_delete_many().collation()) {
                options_builder.append(kvp("collation", *operation.get_delete_many().collation()));
            }
            scoped_bson_t options(options_builder.extract());

            bson_error_t error;
            auto result = libmongoc::bulk_operation_remove_many_with_opts(
                _impl->operation_t, filter.bson(), options.bson(), &error);
            if (!result) {
                throw_exception<logic_error>(error);
            }
            break;
        }
        case write_type::k_replace_one: {
            scoped_bson_t filter(operation.get_replace_one().filter());
            scoped_bson_t replace(operation.get_replace_one().replacement());

            bsoncxx::builder::basic::document options_builder;
            if (operation.get_replace_one().collation()) {
                options_builder.append(kvp("collation", *operation.get_replace_one().collation()));
            }
            if (operation.get_replace_one().upsert()) {
                options_builder.append(kvp("upsert", *operation.get_replace_one().upsert()));
            }
            scoped_bson_t options(options_builder.extract());

            bson_error_t error;
            auto result = libmongoc::bulk_operation_replace_one_with_opts(
                _impl->operation_t, filter.bson(), replace.bson(), options.bson(), &error);
            if (!result) {
                throw_exception<logic_error>(error);
            }
            break;
        }
    }
}

bulk_write::bulk_write(const collection& coll, const options::bulk_write& options)
    : _created_from_collection{true},
      _impl{stdx::make_unique<bulk_write::impl>(libmongoc::collection_create_bulk_operation(
          coll._get_impl().collection_t,
          options.ordered(),
          options.write_concern() ? options.write_concern()->_impl->write_concern_t : nullptr))} {
    if (auto validation = options.bypass_document_validation()) {
        libmongoc::bulk_operation_set_bypass_document_validation(_impl->operation_t, *validation);
    }
}

MONGOCXX_INLINE_NAMESPACE_END
}  // namespace mongocxx
