#include <typeinfo>
#include <cvt/cvt_concepts.h>
#include <cvt/comp/zlib_cvt.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>

#include <support/dump_info.h>
#include <support/verify.h>

void test_runtime_root_cvt_mem_gen_1()
{
    using namespace IOv2;
    dump_info("Test runtime<root_cvt> general case 1...");
    
    {
        using CheckType = runtime_cvt<mem_device<char>, char>;
        static_assert(IOv2::io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
        static_assert(std::is_same_v<CheckType::internal_type, char>);
        static_assert(std::is_same_v<CheckType::external_type, char>);
        static_assert(cvt_cpt::support_put<CheckType>);
        static_assert(cvt_cpt::support_get<CheckType>);
        static_assert(cvt_cpt::support_positioning<CheckType>);
        static_assert(cvt_cpt::support_io_switch<CheckType>);
    }
    
    {
        using CheckType = runtime_cvt<mem_device<char32_t>, char32_t>;
        static_assert(IOv2::io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char32_t>>);
        static_assert(std::is_same_v<CheckType::internal_type, char32_t>);
        static_assert(std::is_same_v<CheckType::external_type, char32_t>);
        static_assert(cvt_cpt::support_put<CheckType>);
        static_assert(cvt_cpt::support_get<CheckType>);
        static_assert(cvt_cpt::support_positioning<CheckType>);
        static_assert(cvt_cpt::support_io_switch<CheckType>);
    }

    dump_info("Done\n");
}

// ──────────────────────────────────────────────────────────────────────────────
// Null-instance tests: every public method must throw cvt_error when m_ptr is
// null.  A runtime_cvt enters the null state after being moved from; the null
// state is also propagated by copy-construction and copy-assignment.
// ──────────────────────────────────────────────────────────────────────────────
void test_runtime_cvt_null_instance()
{
    using namespace IOv2;
    dump_info("Test runtime_cvt: null instance...");

    using RT = runtime_cvt<mem_device<char>, char>;

    // Helper: verify that calling fn() throws cvt_error (not any other type).
    auto must_throw = [](auto&& fn)
    {
        bool threw = false;
        try { fn(); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    };

    // ── Case 1: All 16 public methods throw on a moved-from (null) instance ──
    {
        RT src(rb_root_cvt{mem_device("")});
        RT dst = std::move(src); // src.m_ptr is now null

        char          buf[1] = {};
        const char*   cbuf   = "a";
        cvt_behavior  beh;
        cvt_status    stat;

        must_throw([&] { (void)src.device();              });
        must_throw([&] { src.attach(mem_device<char>{}); });
        must_throw([&] { src.adjust(beh);                 });
        must_throw([&] { src.retrieve(stat);              });
        must_throw([&] { (void)src.is_eof();              });
        must_throw([&] { (void)src.bos();                 });
        must_throw([&] { src.main_cont_beg();             });
        must_throw([&] { (void)src.get(buf, 1);           });
        must_throw([&] { src.put(cbuf, 1);                });
        must_throw([&] { src.flush();                     });
        must_throw([&] { (void)src.tell();                });
        must_throw([&] { src.seek(0);                     });
        must_throw([&] { src.rseek(0);                   });
        must_throw([&] { src.switch_to_get();             });
        must_throw([&] { src.switch_to_put();             });
    }

    // ── Case 2: Copy-constructing from a null instance propagates null ────────
    {
        RT src(rb_root_cvt{mem_device("")});
        RT dst        = std::move(src);  // src is null
        RT propagated = src;             // copy of null → also null

        must_throw([&] { (void)propagated.bos();   });
        must_throw([&] { (void)propagated.tell();  });
        must_throw([&] { (void)propagated.flush(); });
    }

    // ── Case 3: Copy-assigning from a null instance propagates null ───────────
    {
        RT src(rb_root_cvt{mem_device("")});
        RT dst = std::move(src);  // src is null

        RT target(rb_root_cvt{mem_device("x")});  // initially valid
        target = src;                              // assign null → target becomes null

        must_throw([&] { (void)target.bos();   });
        must_throw([&] { (void)target.tell();  });
        must_throw([&] { (void)target.flush(); });
    }

    dump_info("Done\n");
}

void test_runtime_cvt_full_ops()
{
    using namespace IOv2;
    dump_info("Test runtime_cvt: tell/seek/rseek/switch/is_eof/retrieve...");

    {
        runtime_cvt obj(rb_root_cvt{mem_device(std::string("hello world"))});

        VERIFY(obj.bos() == io_status::input);

        // retrieve must not throw
        cvt_status stat;
        obj.retrieve(stat);

        obj.main_cont_beg();

        // is_eof: not at end yet
        VERIFY(!obj.is_eof());

        // read some bytes
        char buf[5] = {};
        VERIFY(obj.get(buf, 5) == 5);

        // tell: position should have advanced
        VERIFY(obj.tell() > 0);

        // seek back to start
        obj.seek(0);
        VERIFY(obj.tell() == 0);

        // rseek(3): reverse-seek to 3 chars before end of stream; just verify no throw
        obj.rseek(3);
        VERIFY(obj.tell() > 0);

        // switch_to_put: m_io_status is input → actual switch (non-idempotent)
        obj.switch_to_put();
        // switch_to_put again: m_io_status is now output → idempotent early return
        obj.switch_to_put();

        // switch_to_get: m_io_status is output → actual switch (non-idempotent)
        obj.switch_to_get();
        // switch_to_get again: m_io_status is now input → idempotent early return
        obj.switch_to_get();

        obj.detach();
    }

    dump_info("Done\n");
}

void test_runtime_cvt_unsupported_ops()
{
    using namespace IOv2;
    dump_info("Test runtime_cvt: unsupported ops throw, idempotent switch...");

    auto must_throw = [](auto fn) {
        bool threw = false;
        try { fn(); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    };

    using ZCvt = Comp::zlib_cvt<rb_root_cvt<mem_device<char>>>;

    // tell/seek/rseek on a zlib_cvt-wrapping runtime_cvt (no positioning support) → throw
    {
        runtime_cvt obj(ZCvt{rb_root_cvt{mem_device("")}, 6});
        must_throw([&] { (void)obj.tell(); });
        must_throw([&] { obj.seek(0); });
        must_throw([&] { obj.rseek(0); });
    }

    // switch_to_get throws when m_io_status is output and kernel has no io_switch
    {
        runtime_cvt obj(ZCvt{rb_root_cvt{mem_device("")}, 6});
        obj.bos();  // returns output; m_io_status = output in runtime_cvt_imp
        must_throw([&] { obj.switch_to_get(); });
    }

    // switch_to_put throws when m_io_status is input and kernel has no io_switch
    {
        std::string compressed;
        {
            ZCvt comp{rb_root_cvt{mem_device("")}, 6};
            comp.bos(); comp.main_cont_beg();
            char data[] = "hi";
            comp.put(data, 2);
            auto [dev, err] = comp.detach();
            compressed = dev.str();
        }
        runtime_cvt obj(ZCvt{rb_root_cvt{mem_device(compressed)}, 6});
        obj.bos();  // returns input; m_io_status = input in runtime_cvt_imp
        must_throw([&] { obj.switch_to_put(); });
    }

    // switch_to_get is idempotent when m_io_status is already input (no throw)
    {
        std::string compressed;
        {
            ZCvt comp{rb_root_cvt{mem_device("")}, 6};
            comp.bos(); comp.main_cont_beg();
            char data[] = "hi";
            comp.put(data, 2);
            auto [dev, err] = comp.detach();
            compressed = dev.str();
        }
        runtime_cvt obj(ZCvt{rb_root_cvt{mem_device(compressed)}, 6});
        obj.bos();           // m_io_status = input
        obj.switch_to_get(); // idempotent: m_io_status == input → returns immediately
    }

    // switch_to_put is idempotent when m_io_status is already output (no throw)
    {
        runtime_cvt obj(ZCvt{rb_root_cvt{mem_device("")}, 6});
        obj.bos();           // m_io_status = output
        obj.switch_to_put(); // idempotent: m_io_status == output → returns immediately
    }

    dump_info("Done\n");
}
