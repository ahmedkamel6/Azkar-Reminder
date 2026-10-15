#pragma once

namespace dhikr {

#define AZKAR_WIDE_DETAIL(value) L##value
#define AZKAR_WIDE(value) AZKAR_WIDE_DETAIL(value)

struct ApplicationMetadata {
    static constexpr const wchar_t* AppName         = L"Azkar Reminder";
    static constexpr const wchar_t* AppVersion      = AZKAR_WIDE(AZKAR_APP_VERSION);
    static constexpr const wchar_t* DeveloperName   = L"Ahmed Kamel";
    static constexpr const wchar_t* DeveloperEmail  = L"ahmedmohamedkamelaretab@gmail.com";
    static constexpr const wchar_t* Copyright       = L"Copyright \x00A9 2026 Ahmed Kamel. All rights reserved.";
    static constexpr const wchar_t* SadaqahMessage  = L"May Allah accept this work as a continuous charity.";

    // Support Details
    static constexpr const wchar_t* VodafoneCashNum     = L"01143995232";
    static constexpr const wchar_t* VodafoneCashLink    = L"http://vf.eg/vfcash?id=mt&qrId=Kke11v";
    static constexpr const wchar_t* BankName            = L"Banque Misr";
    static constexpr const wchar_t* BeneficiaryName     = L"Ahmed Mohamed Kamel";
    static constexpr const wchar_t* IBAN                = L"EG300002042004200383000005970";
    static constexpr const wchar_t* IBANFormatted       = L"EG30 0002 0420 0420 0383 0000 0597 0";
    static constexpr const wchar_t* IBANFullDetails     = L"Bank: Banque Misr\nBeneficiary: Ahmed Mohamed Kamel\nIBAN: EG300002042004200383000005970";
};

#undef AZKAR_WIDE
#undef AZKAR_WIDE_DETAIL

} // namespace dhikr
