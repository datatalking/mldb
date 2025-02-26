/* cell_value_test.cc                                              -*- C++ -*-
   Jeremy Barnes, 24 December 2014
   Copyright (c) 2014 mldb.ai inc.  All rights reserved.

   This file is part of MLDB. Copyright 2015 mldb.ai inc. All rights reserved.

   Test of cell values.
*/

#include "mldb/sql/cell_value.h"
#include "mldb/types/path.h"
#include "mldb/types/value_description.h"
#include "types/annotated_exception.h"

#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include <climits>
#include <random>


using namespace std;

using namespace MLDB;

BOOST_AUTO_TEST_CASE( test_size )
{
    BOOST_CHECK_EQUAL(sizeof(CellValue), 16);
}

BOOST_AUTO_TEST_CASE( test_basics )
{
    BOOST_CHECK_EQUAL(CellValue(0), CellValue(0.0));
//    BOOST_CHECK_LT(CellValue(Utf8String("école")), CellValue(Utf8String("zoo")));
    BOOST_CHECK_EQUAL(CellValue(Utf8String("école")), CellValue(Utf8String("école")));
    // since we instrospect the content of the string we will choose the best encoding
    // depending on the contents
    BOOST_CHECK_EQUAL(CellValue(Utf8String("only ascii")).cellType(), CellValue::ASCII_STRING);
    BOOST_CHECK_EQUAL(CellValue(1.0).cellType(), CellValue::INTEGER);
    BOOST_CHECK_EQUAL(CellValue(1.1).cellType(), CellValue::FLOAT);
    //BOOST_CHECK_NO_THROW(CellValue(L"Crédit Agricole Suisse Open Gstaad").cellType());
    //BOOST_CHECK_NO_THROW(CellValue(L"Mutua Madrileña Madrid Open").cellType());
    {
        MLDB_TRACE_EXCEPTIONS(false);
        BOOST_CHECK_THROW(CellValue(std::string("Crédit Agricole Suisse Open Gstaad")).cellType(), MLDB::Exception);
        BOOST_CHECK_THROW(CellValue(std::string("Mutua Madrileña Madrid Open")).cellType(),
                          MLDB::Exception);
    }
    BOOST_CHECK_EQUAL(CellValue(Utf8String("Crédit Agricole Suisse Open Gstaad")).cellType(), CellValue::UTF8_STRING);
    BOOST_CHECK_EQUAL(CellValue(Utf8String("Mutua Madrileña Madrid Open")).cellType(), CellValue::UTF8_STRING);
    BOOST_CHECK_EQUAL(CellValue().cellType(), CellValue::EMPTY);
    BOOST_CHECK_EQUAL(CellValue("").cellType(), CellValue::ASCII_STRING);
    BOOST_CHECK_EQUAL(CellValue("1").cellType(), CellValue::ASCII_STRING);
    BOOST_CHECK_EQUAL(CellValue("-1").cellType(), CellValue::ASCII_STRING);
    BOOST_CHECK_EQUAL(CellValue("+1").cellType(), CellValue::ASCII_STRING);
    BOOST_CHECK_NE(CellValue(), CellValue(""));
    BOOST_CHECK_NE(CellValue(0), CellValue("0"));
    BOOST_CHECK_NE(CellValue(0), CellValue("0.0"));
    BOOST_CHECK_NE(CellValue(0), CellValue("+0.0"));
    BOOST_CHECK_NE(CellValue(0), CellValue("-0.0"));
    BOOST_CHECK_LT(CellValue(-1), CellValue(0));
    BOOST_CHECK_LT(CellValue(), CellValue(0));
    BOOST_CHECK_LT(CellValue(1), CellValue(1.1));
    
    auto nan = std::numeric_limits<float>::quiet_NaN();

    BOOST_CHECK_EQUAL(CellValue(nan), CellValue(nan));
    BOOST_CHECK_LT(CellValue(nan), CellValue(0));
    BOOST_CHECK(!(CellValue(nan) < CellValue(nan)));
    
    // Corner cases that should be tested:
    // 1.  Not a number, including case variants
    // 2.  Infinities, including case variants
    // 3.  64 bit integers that can't be exactly represented by a double
    // 4.  numbers between LONG_LONG_MAX and ULONG_LONG_MAX
    // 5.  Scientific notation
}

void checkOrdering(const CellValue & v1,
                   const CellValue & v2)
{
    bool eq = v1 == v2;
    bool lt = v1 <  v2;
    bool gt = v2 <  v1;

    if (eq)
        BOOST_CHECK_EQUAL(v1.toString(), v2.toString());

    if (eq + lt + gt != 1) {
        cerr << "ordering error with " << v1 << " and " << v2 << endl;
    }

    BOOST_CHECK_EQUAL(eq + lt + gt, 1);
}

BOOST_AUTO_TEST_CASE( test_ordering )
{
    vector<CellValue> values { CellValue(), std::numeric_limits<float>::quiet_NaN(),
                                -1.5, 1, 2, 2.3, 3, 3.0, 3.5, "", "one", "three",
                                "three hundred and forty-five thousand", "two" };

    for (auto & v1: values) {
        for (auto & v2: values) {
            checkOrdering(v1, v2);
        }
    }

    for (unsigned i = 0;  i < 10;  ++i) {
        vector<CellValue> unsorted = values;
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(unsorted.begin(), unsorted.end(), g);
        std::sort(unsorted.begin(), unsorted.end());

        BOOST_CHECK_EQUAL_COLLECTIONS(unsorted.begin(), unsorted.end(),
                                      values.begin(), values.end());
    }
}

BOOST_AUTO_TEST_CASE( test_printing )
{
    BOOST_CHECK_NE(CellValue("1.100000000"), CellValue("1.1"));
    BOOST_CHECK_EQUAL(CellValue("1.1").toString(), "1.1");
    BOOST_CHECK_EQUAL(CellValue("-1.1").toString(), "-1.1");
    BOOST_CHECK_EQUAL(CellValue("1.1e100").toString(), "1.1e100");
    BOOST_CHECK_EQUAL(CellValue("1e100").toString(), "1e100");
    BOOST_CHECK_EQUAL(CellValue("1.1e-100").toString(), "1.1e-100");
    BOOST_CHECK_EQUAL(CellValue("1e-100").toString(), "1e-100");
    BOOST_CHECK_EQUAL(CellValue("0.1").toString(), "0.1");
    BOOST_CHECK_EQUAL(CellValue("0.01").toString(), "0.01");

    BOOST_CHECK_EQUAL(CellValue("long long long long").toString(), "long long long long");
}

BOOST_AUTO_TEST_CASE( test_64_bit_range )
{
    BOOST_CHECK_EQUAL(CellValue(std::numeric_limits<uint64_t>::max()).toUInt(),
                      std::numeric_limits<uint64_t>::max());
    {
        MLDB_TRACE_EXCEPTIONS(false);
        auto v = [&] () { return CellValue(std::numeric_limits<uint64_t>::max()).toInt(); };
        BOOST_CHECK_THROW(v(), MLDB::Exception);
    }
}

BOOST_AUTO_TEST_CASE( test_date )
{
    BOOST_CHECK_EQUAL(CellValue(Date::fromSecondsSinceEpoch(0.0)).toString(),
                      "1970-01-01T00:00:00Z");
    BOOST_CHECK_EQUAL(CellValue(Date::fromSecondsSinceEpoch(0.1)).toString(),
                      "1970-01-01T00:00:00.1Z");
    BOOST_CHECK_EQUAL(CellValue(Date::fromSecondsSinceEpoch(0.002)).toString(),
                      "1970-01-01T00:00:00.002Z");
    BOOST_CHECK_EQUAL(CellValue(Date::fromSecondsSinceEpoch(0.333)).toString(),
                      "1970-01-01T00:00:00.333Z");

    Date now(Date::now());
    CellValue ts1(now);

    BOOST_CHECK_EQUAL(CellValue(ts1.toString()).coerceToTimestamp(),
                      ts1);

    BOOST_CHECK_EQUAL(jsonDecode<CellValue>(jsonEncode(ts1)),
                      ts1);

    BOOST_CHECK_EQUAL(CellValue(jsonDecodeStr<Date>(string("\"2015-10-06T20:52:18.842Z\""))).toString(),
                      "2015-10-06T20:52:18.842Z");

    BOOST_CHECK_EQUAL(jsonEncodeStr(CellValue(jsonDecodeStr<Date>(string("\"2015-10-06T20:52:18.842Z\"")))),
                      "{\"ts\":\"2015-10-06T20:52:18.842Z\"}");
}

BOOST_AUTO_TEST_CASE(test_blob)
{
    char blobData[] = "\1\1\2\3\4\5\0";
    string blobContents(blobData, blobData + 7);

    BOOST_CHECK_EQUAL(blobContents.size(), 7);

    CellValue blob = CellValue::blob(blobContents);
    
    BOOST_CHECK_EQUAL(blob.cellType(), CellValue::BLOB);
    BOOST_CHECK(blob.isBlob());
    BOOST_CHECK_EQUAL(blob, blob);
    BOOST_CHECK(!(blob != blob));
    BOOST_CHECK(!(blob < blob));
    
    BOOST_CHECK_EQUAL(blob.blobLength(), blobContents.size());
    BOOST_CHECK_EQUAL(string(blob.blobData(), blob.blobData() + blob.blobLength()),
                      blobContents);

    BOOST_CHECK_EQUAL(jsonEncodeStr(blob), "{\"blob\":[1,1,2,3,4,5,0]}");
    BOOST_CHECK_EQUAL(jsonEncodeStr(CellValue::blob("")),
                      "{\"blob\":[]}");
    BOOST_CHECK_EQUAL(jsonEncodeStr(jsonDecode<CellValue>(jsonEncode(CellValue::blob("")))),
                      jsonEncodeStr(CellValue::blob("")));
    BOOST_CHECK_EQUAL(jsonEncodeStr(CellValue::blob("hello\1")),
                      "{\"blob\":[\"hello\",1]}");
    BOOST_CHECK_EQUAL(jsonEncodeStr(jsonDecode<CellValue>(jsonEncode(CellValue::blob("hello\1")))),
                      jsonEncodeStr(CellValue::blob("hello\1")));

    BOOST_CHECK_EQUAL(jsonEncodeStr(jsonDecodeStr<CellValue>(jsonEncodeStr(blob))),
                      jsonEncodeStr(blob));
}

BOOST_AUTO_TEST_CASE (test_int_to_string)
{
    BOOST_CHECK_EQUAL(CellValue(0).toString(), "0");
    BOOST_CHECK_EQUAL(CellValue(1).toString(), "1");
    BOOST_CHECK_EQUAL(CellValue(10).toString(), "10");
    BOOST_CHECK_EQUAL(CellValue(1000).toString(), "1000");
    BOOST_CHECK_EQUAL(CellValue(-1).toString(), "-1");
    BOOST_CHECK_EQUAL(CellValue(-10).toString(), "-10");
    BOOST_CHECK_EQUAL(CellValue(std::numeric_limits<int64_t>::max()).toString(),
                      std::to_string(std::numeric_limits<int64_t>::max()));
    BOOST_CHECK_EQUAL(CellValue(std::numeric_limits<uint64_t>::max()).toString(),
                      std::to_string(std::numeric_limits<uint64_t>::max()));
    BOOST_CHECK_EQUAL(CellValue(std::numeric_limits<int64_t>::min()).toString(),
                      std::to_string(std::numeric_limits<int64_t>::min()));
}

BOOST_AUTO_TEST_CASE (test_realistic_float)
{
    constexpr const char * realisticFloat1 = "-0.38860246539115906";

    auto cell1 = CellValue::parse(realisticFloat1, 
                                  strlen(realisticFloat1), 
                                  STRING_IS_VALID_ASCII);

    BOOST_CHECK_EQUAL(cell1.cellType(), CellValue::FLOAT);

    constexpr const char * realisticFloat2 = "-0.38860246539115906123454634";

    auto cell2 = CellValue::parse(realisticFloat2, 
                                  strlen(realisticFloat1), // intended - must not read passed the length
                                  STRING_IS_VALID_ASCII);

    BOOST_CHECK_EQUAL(cell2.cellType(), CellValue::FLOAT);

    BOOST_CHECK_EQUAL(cell1, cell2);

    constexpr const char * veryLongFloat = 
        "0.0000000000000000000000000000000000000000000000000000000000000000000000000000000023942190";

    auto cell3 = CellValue::parse(veryLongFloat, 
                                  strlen(veryLongFloat),
                                  STRING_IS_VALID_ASCII);

    BOOST_CHECK_EQUAL(cell3.cellType(), CellValue::FLOAT);

    constexpr const char * veryVeryLongFloat = 
        "0.00000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "00000000000023942190";

    auto cell4 = CellValue::parse(veryVeryLongFloat, 
                                  strlen(veryVeryLongFloat),
                                  STRING_IS_VALID_ASCII);

    BOOST_CHECK_EQUAL(cell4.cellType(), CellValue::FLOAT);

    constexpr const char * veryVeryVeryLongFloat = 
        "0.00000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "23942190";

    auto cell5 = CellValue::parse(veryVeryVeryLongFloat, 
                                  strlen(veryVeryVeryLongFloat),
                                  STRING_IS_VALID_ASCII);

    // the value is rounded to 0
    BOOST_CHECK_EQUAL(cell5.cellType(), CellValue::INTEGER);
}

BOOST_AUTO_TEST_CASE (test_realistic_int)
{
    constexpr const char * realisticInt1 = "-38860246539115906";

    auto cell1 = CellValue::parse(realisticInt1, 
                                  strlen(realisticInt1), 
                                  STRING_IS_VALID_ASCII);

    BOOST_CHECK_EQUAL(cell1.cellType(), CellValue::INTEGER);

    constexpr const char * smallerThanMinInt = "-38860246539115906123454634";

    auto cell2 = CellValue::parse(smallerThanMinInt, 
                                  strlen(realisticInt1), // intended - must not read passed the length
                                  STRING_IS_VALID_ASCII);

    BOOST_CHECK_EQUAL(cell2.cellType(), CellValue::INTEGER);
    BOOST_CHECK_EQUAL(cell1, cell2);

    constexpr const char * largerThanMaxInt = "+38860246539115906123454634";

    auto cell3 = CellValue::parse(largerThanMaxInt, 
                                  strlen(largerThanMaxInt),
                                  STRING_IS_VALID_ASCII);

    BOOST_CHECK_EQUAL(cell3.cellType(), CellValue::INTEGER);
    BOOST_CHECK_EQUAL(cell3.toInt(), LLONG_MAX);

    auto cell4 = CellValue::parse(smallerThanMinInt, 
                                  strlen(smallerThanMinInt),
                                  STRING_IS_VALID_ASCII);

    BOOST_CHECK_EQUAL(cell4.cellType(), CellValue::INTEGER);
    BOOST_CHECK_EQUAL(cell4.toInt(), LLONG_MIN);
}


BOOST_AUTO_TEST_CASE (test_realistic_uint)
{
    constexpr const char * realisticUInt1 = "38860246539115906";

    auto cell1 = CellValue::parse(realisticUInt1, 
                                  strlen(realisticUInt1), 
                                  STRING_IS_VALID_ASCII);

    BOOST_CHECK_EQUAL(cell1.cellType(), CellValue::INTEGER);

    constexpr const char * realisticUInt2 = "38860246539115906123454634";

    auto cell2 = CellValue::parse(realisticUInt2, 
                                  strlen(realisticUInt1), // intended - must not read passed the length
                                  STRING_IS_VALID_ASCII);

    BOOST_CHECK_EQUAL(cell2.cellType(), CellValue::INTEGER);
    BOOST_CHECK_EQUAL(cell1, cell2);
}

template<typename T>
std::function<bool(T const&)>
exceptionCheck(const std::string & pattern) {
    return [=] ( T const& ex )
        {
            cout << ex.what() << endl;
            return string(ex.what()).find(pattern) != std::string::npos;
        };
}

BOOST_AUTO_TEST_CASE (test_exception_messages)
{
    auto cv = CellValue(Utf8String("françois"));

    auto returnExceptCheck = exceptionCheck<std::exception>("value 'fran");
    MLDB_TRACE_EXCEPTIONS(false);
    BOOST_CHECK_EXCEPTION( cv.toString(), std::exception, returnExceptCheck);
    BOOST_CHECK_EXCEPTION( cv.toDouble(), std::exception, returnExceptCheck);
    BOOST_CHECK_EXCEPTION( cv.toInt(), std::exception, returnExceptCheck);

    auto returnHttpExcptCheck = exceptionCheck<AnnotatedException>("value 'fran");
    BOOST_CHECK_EXCEPTION( cv.toTimestamp(), AnnotatedException, returnHttpExcptCheck);
    BOOST_CHECK_EXCEPTION( cv.toMonthDaySecond(), AnnotatedException, returnHttpExcptCheck);
    BOOST_CHECK_EXCEPTION( cv.blobData(), AnnotatedException, returnHttpExcptCheck);
    BOOST_CHECK_EXCEPTION( cv.blobLength(), AnnotatedException, returnHttpExcptCheck);

    // make sure we'll trim the exception
    cv = CellValue(Utf8String("éabcdefasdfasdeifjshifjsifjsijasdfweoinnvoijoiwnvoijwef"
                              "abcdefasdfasdeifjshifjsifjsijasdfweoinnvoijoiwnvoijwef"
                              "abcdefasdfasdeifjshifjsifjsijasdfweoinnvoijoiwnvoijwef"
                              "abcdefasdfasdeifjshifjsifjsijasdfweoinnvoijoiwnvoijwef"
                              "abcdefasdfasdeifjshifjsifjsijasdfweoinnvoijoiwnvoijwef"));
    BOOST_CHECK_EXCEPTION( cv.toString(), std::exception, exceptionCheck<std::exception>("... (trimmed)'"));
}

BOOST_AUTO_TEST_CASE (test_path)
{
    {
        Path p1(PathElement(1));
        auto cv = CellValue(p1);
        Path p2 = cv.coerceToPath();
        
        BOOST_CHECK_EQUAL(p1, p2);
    }

    {
        const std::string s("[\"d5\",1]");
        PathElement e(s);
        BOOST_CHECK_EQUAL(e.toUtf8String(), s);
        BOOST_CHECK_EQUAL(e.toEscapedUtf8String(), "\"[\"\"d5\"\",1]\"");
        cerr << jsonEncodeStr(e) << endl;
        Path p1(e);
        cerr << jsonEncodeStr(p1) << endl;
        auto cv = CellValue(p1);
        cerr << jsonEncodeStr(cv) << endl;
        Path p2 = cv.coerceToPath();
        
        BOOST_CHECK_EQUAL(p1, p2);
    }



}

BOOST_AUTO_TEST_CASE (test_sorting_absolute_order)
{
    // This list should have examples of each type of CellValue, as it
    // tests that there is a global absolute order.
    std::vector<CellValue> vals = {
        CellValue(),
        0,
        1,
        -1,
        0.5,
        -0.5,
        std::numeric_limits<double>::quiet_NaN(),
        INFINITY,
        -INFINITY,
        std::numeric_limits<double>::min(),
        std::numeric_limits<double>::max(),
        std::numeric_limits<uint64_t>::max(),
        std::numeric_limits<int64_t>::min(),
        "",
        "1",
        "one",
        "123",
        "123456789abcdefghijllmnop",
        Utf8String("123456789abcdefg1235dubé"),
        Utf8String("é"),
        Date::now(),
        Date(),
        Date::negativeInfinity(),
        Date::positiveInfinity(),
        Date::notADate(),
        CellValue::blob("123"),
        CellValue::blob("12345678123asdasdberassada"),
        CellValue::blob("12345678123\0asdasdberassada", 25),
        CellValue::blob(""),
        Path(),
        Path("1"),
        Path("a"),
        Path(""),
        Path::parse("a.b"),
        Path::parse("1.asdasdsadasasd.2.sadasdsadsadasdsdasd"),
        Path(PathElement("[\"d5\",1]")),
        CellValue::fromMonthDaySecond(0, 0, 0.0),
        CellValue::fromMonthDaySecond(100, 12, 34.56),
        CellValue::fromMonthDaySecond(100, 12, std::numeric_limits<double>::quiet_NaN()),
        CellValue::fromMonthDaySecond(100, 12, INFINITY),
        CellValue::fromMonthDaySecond(100, 12, -INFINITY)
    };

    std::sort(vals.begin(), vals.end());

    auto fmtWithLength = CellValue::serializationFormat(true);
    auto fmtWithoutLength = CellValue::serializationFormat(false);
    
    for (auto & v: vals) {
        cerr << jsonEncodeStr(v) << endl;

        size_t bytesRequiredWithLength = v.serializedBytes(true);
        size_t bytesRequiredWithoutLength = v.serializedBytes(false);

        std::string buf;
        buf.resize(std::max(bytesRequiredWithLength,
                            bytesRequiredWithoutLength));

        char * pos = v.serialize(&buf[0], bytesRequiredWithLength, true);

        BOOST_CHECK_EQUAL((const void *)pos,
                          (const void *)(buf.data() + bytesRequiredWithLength));

        ssize_t bytes;
        CellValue output;

        std::tie(output, bytes)
            = CellValue::reconstitute(buf.data(), bytesRequiredWithLength,
                                      fmtWithLength,
                                      true /* exact available */);
        BOOST_CHECK_EQUAL(v, output);
        BOOST_CHECK_EQUAL(jsonEncodeStr(v), jsonEncodeStr(output));
        BOOST_CHECK_EQUAL(bytes, bytesRequiredWithLength);

        output = CellValue();
        pos = v.serialize(&buf[0], buf.length(), false);

        BOOST_CHECK_EQUAL((const void *)pos,
                          (const void *)
                          (buf.data() + bytesRequiredWithoutLength));
        
        std::tie(output, bytes)
            = CellValue::reconstitute(buf.data(), buf.length() * 123 /* fake */,
                                      fmtWithoutLength,
                                      false /* exact available */);

        BOOST_CHECK_EQUAL(v, output);
        BOOST_CHECK_EQUAL(jsonEncodeStr(v), jsonEncodeStr(output));
        BOOST_CHECK_EQUAL(bytes, bytesRequiredWithoutLength);
    }

    for (size_t i = 0;  i < vals.size();  ++i) {
        // check that comparing this with all before gives the right
        // result.

        //cerr << "testing " << jsonEncodeStr(vals[i]) << endl;

        BOOST_CHECK_EQUAL(vals[i], vals[i]);
        BOOST_CHECK_EQUAL(vals[i].compare(vals[i]), 0);

        for (size_t j = 0;  j < i;  ++j) {
            //cerr << "  comparing lt " << jsonEncodeStr(vals[j]) << endl;
            BOOST_CHECK_EQUAL(vals[j].compare(vals[i]), -1);
            BOOST_CHECK_LT(vals[j], vals[i]);
            BOOST_CHECK_LE(vals[j], vals[i]);
            BOOST_CHECK_NE(vals[j], vals[i]);
            BOOST_CHECK_GT(vals[i], vals[j]);
            BOOST_CHECK_GE(vals[i], vals[j]);
        }

        for (size_t j = i + 1;  j < vals.size();  ++j) {
            //cerr << "  comparing gt " << jsonEncodeStr(vals[j]) << endl;
            BOOST_CHECK_EQUAL(vals[j].compare(vals[i]), 1);
            BOOST_CHECK_LT(vals[i], vals[j]);
            BOOST_CHECK_LE(vals[i], vals[j]);
            BOOST_CHECK_NE(vals[i], vals[j]);
            BOOST_CHECK_GT(vals[j], vals[i]);
            BOOST_CHECK_GE(vals[j], vals[i]);
        }
    }
}
