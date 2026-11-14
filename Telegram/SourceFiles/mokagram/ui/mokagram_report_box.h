// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOKAGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mokagram-id: 35bb9826
// Mokagram: окно жалобы в стиле окон Mogram.
#pragma once

namespace Ui {
class GenericBox;
} // namespace Ui

namespace Api {
struct ReportResult;
} // namespace Api

namespace Data {
struct ReportInput;
} // namespace Data

namespace Mokagram::Report {

void FlowBox(
	not_null<Ui::GenericBox*> box,
	const Api::ReportResult &result,
	const Data::ReportInput &input,
	Fn<void(Data::ReportInput)> choose);

} // namespace Mokagram::Report
