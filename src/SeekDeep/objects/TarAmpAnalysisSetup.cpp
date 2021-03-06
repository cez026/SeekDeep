/*
 * TarAmpPEAnalysisSetup.cpp
 *
 *  Created on: Nov 25, 2016
 *      Author: nick
 */


#include "TarAmpAnalysisSetup.hpp"


namespace bibseq {



bool TarAmpAnalysisSetup::TarAmpPars::checkForStitcher(VecStr & warnings) const {
	if (!bib::sys::hasSysCommand(stitcherCmd)) {
		std::stringstream ss;
		ss << __PRETTY_FUNCTION__ << ": error, need to have " << stitcherCmd
				<< " in path" << "\n";
		warnings.emplace_back(ss.str());
		return false;
	}
	return true;
}

bool TarAmpAnalysisSetup::TarAmpPars::checkForZcat(VecStr & warnings) const {
	if (!bib::sys::hasSysCommand(zcatCmd)) {
		std::stringstream ss;
		ss << __PRETTY_FUNCTION__ << ": error, need to have " << zcatCmd
				<< " in path" << "\n";
		warnings.emplace_back(ss.str());
		return false;
	}
	return true;
}


bool TarAmpAnalysisSetup::TarAmpPars::checkForOutDir(VecStr & warnings) const {
	if (bfs::exists(outDir)) {
		std::stringstream ss;
		ss << __PRETTY_FUNCTION__ << ": error, out directory " << "\""
				<< bib::bashCT::boldRed(outDir.string()) << "\"" << " already exists"
				<< ", must remove by hand will not overwrite\n";
		warnings.emplace_back(ss.str());
		return false;
	}
	return true;
}

bool TarAmpAnalysisSetup::TarAmpPars::checkIfFnpExists(const bfs::path & fnp,
		VecStr & warnings) {
	if (!bfs::exists(fnp)) {
		std::stringstream ss;
		ss << bib::bashCT::boldRed(fnp.string()) << " doesn't exist" << "\n";
		warnings.emplace_back(ss.str());
		return false;
	}
	return true;
}


bool TarAmpAnalysisSetup::TarAmpPars::techIs454()const{
	return "454" == technology;
}

bool TarAmpAnalysisSetup::TarAmpPars::techIsIllumina()const{
	return "illumina" == technology;
}

bool TarAmpAnalysisSetup::TarAmpPars::techIsIonTorrent()const{
	return "iontorrent" == technology;
}


bool TarAmpAnalysisSetup::TarAmpPars::checkForRequiredFnpPars(
		VecStr & warnings) const {
	bool status = true;
	status = status && checkForOutDir(warnings);
	status = status && checkIfFnpExists(samplesNamesFnp, warnings);
	status = status && checkIfFnpExists(inputDir, warnings);
	status = status && checkIfFnpExists(idFile, warnings);
	if (byIndex) {
		status = status && checkIfFnpExists(targetsToIndexFnp, warnings);
	}
	return status;
}

bool TarAmpAnalysisSetup::TarAmpPars::allChecks(VecStr & warnings) const {
	bool status = true;
	status = status && checkForRequiredFnpPars(warnings);
	status = status && checkForOptionalFnpPars(warnings);
	status = status && checkForStitcher(warnings);
	status = status && checkForZcat(warnings);
	return status;
}



bool TarAmpAnalysisSetup::TarAmpPars::checkForOptionalFnpPars(VecStr & warnings) const{
	bool status = true;
	if(refSeqsDir != ""){
		status = status && checkIfFnpExists(refSeqsDir, warnings);
		if(bfs::exists(refSeqsDir) && !bfs::is_directory(refSeqsDir)){
			std::stringstream ss;
			ss << bib::bashCT::boldRed(refSeqsDir.string()) << " is not a directory, the ref seqs directory should be, well, a directory" << "\n";
			warnings.emplace_back(ss.str());
			status = false;
		}
	}
	if(lenCutOffsFnp != ""){
		status = status && checkIfFnpExists(lenCutOffsFnp, warnings);
	}
	if(groupMeta != ""){
		status = status && checkIfFnpExists(groupMeta, warnings);
	}
	return status;
}




TarAmpAnalysisSetup::TarAmpAnalysisSetup(const TarAmpPars & pars) :
		pars_(pars) {
	dir_ = pars.outDir;
	bib::files::makeDir(bib::files::MkdirPar(dir_.string()));
	infoDir_ = bib::files::makeDir(dir_.string(), bib::files::MkdirPar("info"));
	logsDir_ = bib::files::makeDir(dir_.string(), bib::files::MkdirPar("logs"));
	idsDir_ = bib::files::makeDir(infoDir_.string(), bib::files::MkdirPar("ids"));
	reportsDir_ = bib::files::makeDir(dir_.string(),
			bib::files::MkdirPar("reports"));
	serverConfigsDir_ = bib::files::makeDir(dir_.string(), bib::files::MkdirPar("serverConfigs"));

	//add ids and primers
	idsMids_ = std::make_unique<PrimersAndMids>(pars_.idFile);
	if(idsMids_->containsMids()){
		idsMids_->checkMidNamesThrow();
	}
	//add sample names
	addSamplesNames(pars_.samplesNamesFnp);
	writeSampleNamesFile();
	if(pars_.byIndex){
		addIndexToTargetsNames(pars_.targetsToIndexFnp);
		bfs::copy(pars_.targetsToIndexFnp, bib::files::make_path(infoDir_, "indexToTargets.tab.txt"));
	}
	//check for incompatible set ups
	if (!idsMids_->containsMids() && pars_.byIndex) {
		std::stringstream ss;
		ss << __PRETTY_FUNCTION__
				<< ": error, if input files don't have MID still, then the sample file"
				<< pars_.samplesNamesFnp << " should be by target and not index" << "\n";
		throw std::runtime_error { ss.str() };
	}

	if (idsMids_->containsMids() && !pars_.byIndex) {
		std::stringstream ss;
		ss << __PRETTY_FUNCTION__
				<< ": error, if input files have MIDs still, then the sample file"
				<< pars_.samplesNamesFnp << " should be by index and not by target" << "\n";
		throw std::runtime_error { ss.str() };
	}

	//add meta data if available
	if("" != pars.groupMeta){
		addGroupingMetaData(pars.groupMeta);
		bfs::copy(groupMetaData_->groupingsFile_,
				bib::files::make_path(infoDir_, "groupMeta.tab.txt"));
	}
	//add ref seqs if provided
	if("" != pars.refSeqsDir){
		addRefSeqs(pars.refSeqsDir);
	}
	//add len cut offs if provided
	if("" != pars.lenCutOffsFnp){
		addLenCutOffs(pars.lenCutOffsFnp);
	}

	auto targets = getTargets();
	VecStr failedTargets;
	for(const auto & tar : targets){
		if(std::string::npos != tar.find("_")){
			failedTargets.emplace_back(tar);
		}
	}
	if(!failedTargets.empty()){
		std::stringstream ss;
		ss << __PRETTY_FUNCTION__
				<< ": error, target names shouldn't have underscores in them (this makes downstream patern matching and such easier) replace with -" << "\n";
		ss << "The following targets should be renamed" << "\n";
		ss << bib::conToStr(failedTargets, ", ") << "\n";
		throw std::runtime_error { ss.str() };
	}
}

void TarAmpAnalysisSetup::addIndexToTargetsNames(
		const bfs::path & targetsToIndexFnp) {
	table targetsToIndexTab(targetsToIndexFnp.string(), "\t", true);
	std::set<std::string> tarsSet;
	for (const auto & row : targetsToIndexTab.content_) {
		auto tars = tokenizeString(row[targetsToIndexTab.getColPos("targets")],
				",");
		auto index = row[targetsToIndexTab.getColPos("index")];
		if (bib::in(index, indexToTars_)) {
			std::stringstream ss;
			ss << __PRETTY_FUNCTION__ << ": error already have targets for index: "
					<< index << "\n";
			ss << "Check to see if there are repeat values in " << targetsToIndexFnp
					<< "\n";
			throw std::runtime_error { ss.str() };
		}
		bib::sort(tars);
		indexToTars_[index] = tars;
		tarsSet.insert(tars.begin(), tars.end());
	}
	auto indexes = getIndexes();
	VecStr missingIndex;
	for (const auto & index : indexes) {
		if (!bib::in(index, indexToTars_)) {
			missingIndex.emplace_back(index);
		}
	}
	if (!missingIndex.empty()) {
		std::stringstream ss;
		ss << __PRETTY_FUNCTION__
				<< ": error, missing the following index from the index to targets file "
				<< targetsToIndexFnp << "\n";
		ss << "Targets: " << bib::conToStr(missingIndex, ", ") << "\n";
		ss << "Indexes Read In: "
				<< bib::conToStr(bib::getVecOfMapKeys(indexToTars_), ", ") << "\n";
		throw std::runtime_error { ss.str() };
	}
}



TarAmpAnalysisSetup::Sample::Sample(const std::string & name) :
		name_(name) {
}

void TarAmpAnalysisSetup::Sample::addRep(const std::string & rep) {
	if (bib::in(rep, reps_)) {
		std::stringstream ss;
		ss << __PRETTY_FUNCTION__ << " error, sample " << name_
				<< " already has rep name " << rep << "\n";
		throw std::runtime_error { ss.str() };
	}
	reps_.emplace_back(rep);
}

void TarAmpAnalysisSetup::Sample::addReps(const VecStr & reps) {
	for (const auto & rep : reps) {
		addRep(rep);
	}
}

VecStr TarAmpAnalysisSetup::Sample::getReps() const {
	return reps_;
}

TarAmpAnalysisSetup::Samples::Samples(const std::string & target): target_(target){};


bool TarAmpAnalysisSetup::Samples::hasSample(const std::string & sample) {
	return samples_.end() != samples_.find(sample);
}

void TarAmpAnalysisSetup::Samples::addSample(const std::string & sample) {
	if (hasSample(sample)) {
		std::stringstream ss;
		ss << __PRETTY_FUNCTION__ << " error, already have " << sample << " for " << target_ << "\n";
		ss << "With input: " << bib::conToStr(samples_.at(sample).reps_ , ", ")<< "\n";
		throw std::runtime_error { ss.str() };
	}
	samples_.emplace(sample, Sample{sample});
}

void TarAmpAnalysisSetup::Samples::addSample(const std::string & sample, const VecStr & reps) {
	addSample(sample);
	samples_.find(sample)->second.addReps(reps);
}

VecStr TarAmpAnalysisSetup::Samples::getSamples() const {
	return bib::getVecOfMapKeys(samples_);
}

std::vector<std::string> TarAmpAnalysisSetup::Samples::getReps() const {
	std::vector<std::string> ret;
	for (const auto & samp : samples_) {
		ret.insert(ret.end(), samp.second.reps_.begin(), samp.second.reps_.end());
	}
	return ret;
}

std::set<std::string> TarAmpAnalysisSetup::getSamples() const {
	std::set<std::string> ret;
	for (const auto & tar : samples_) {
		auto samps = tar.second.getSamples();
		ret.insert(samps.begin(), samps.end());
	}
	return ret;
}

std::vector<std::string> TarAmpAnalysisSetup::getReps() const {
	std::set<std::string> ret;
	for (const auto & tar : samples_) {
		auto reps = tar.second.getReps();
		ret.insert(reps.begin(), reps.end());
	}
	return VecStr{ret.begin(), ret.end()};
}

void TarAmpAnalysisSetup::addGroupingMetaData(const bfs::path & groupingsFileFnp) {
	groupMetaData_ = std::make_unique<MultipleGroupMetaData>(
			bib::files::normalize(groupingsFileFnp), getSamples());
}

void TarAmpAnalysisSetup::addGroupingsFile()const{
	if(nullptr != groupMetaData_){
		bfs::copy(groupMetaData_->groupingsFile_, bib::files::make_path(infoDir_, "groupingMetaData.tab.txt"));
	}
}

void TarAmpAnalysisSetup::writeSampleNamesFile() const {
	std::ofstream sampleNamesFile;
	openTextFile(sampleNamesFile,
			OutOptions(bib::files::make_path(infoDir_, "sampNames").string(),
					".tab.txt"));
	auto keys = getVectorOfMapKeys(samples_);
	bib::sort(keys);
	for (const auto & tarKey : keys) {
		auto sampKeys = bib::getVecOfMapKeys(samples_.at(tarKey).samples_);
		bib::sort(sampKeys);
		for (const auto & sampKey : sampKeys) {
			sampleNamesFile << tarKey << "\t" << sampKey;
			for (auto rep : samples_.at(tarKey).samples_.at(sampKey).reps_) {
				if (!bib::beginsWith(rep, "MID")) {
					rep = "MID" + rep;
				}
				sampleNamesFile << "\t" << rep;
			}
			sampleNamesFile << std::endl;
		}
	}

}

VecStr TarAmpAnalysisSetup::getTargets() const {
	if(pars_.byIndex){
		std::set<std::string> tarsSet;
		for(const auto & indToTar : indexToTars_){
			tarsSet.insert(indToTar.second.begin(), indToTar.second.end());
		}
		return VecStr{tarsSet.begin(), tarsSet.end()};
	}else{
		return bib::getVecOfMapKeys(samples_);
	}
}

VecStr TarAmpAnalysisSetup::getIndexes() const {
	return bib::getVecOfMapKeys(samples_);
}



void TarAmpAnalysisSetup::addSamplesNames(const bfs::path & samplesNamesFnp){
	VecStr warnings;
	table samplesNamesInputTab(samplesNamesFnp.string());
	std::vector<VecStr> finalSamps;
	uint32_t rowCount = 0;
	for(const auto & row : samplesNamesInputTab.content_){
		++rowCount;
		if(row.empty()){
			continue;
		}
		if(0 != row.front().size() && '#' == row.front().front()){
			continue;
		}
		if(row.size() < 3){
			std::stringstream ss;
			ss << __PRETTY_FUNCTION__ << ": error each row should have at least 3 columns, not " << row.size() << " for row \n";
			ss << "Row Number(1 based pos): " << rowCount << " - " << bib::conToStr(row, "\t") << "\n";
			warnings.emplace_back(ss.str());
			continue;
		}
		std::vector<std::string> nonEmptyRow;
		for(const auto & colPos : iter::range(row.size())){
			if(colPos < 2 && row[colPos].empty()){
				std::stringstream ss;
				ss << __PRETTY_FUNCTION__ << ": error column " << colPos << " can't be an empty string" << "\n";
				ss << "Row Number(1 based pos): " << rowCount << " - " << bib::conToStr(row, "\t") << "\n";
				warnings.emplace_back(ss.str());
				continue;
			}
			if(!row[colPos].empty()){
				nonEmptyRow.emplace_back(row[colPos]);
			}
		}
		finalSamps.emplace_back(nonEmptyRow);
	}
	if(!warnings.empty()){
		std::stringstream ss;
		ss << __PRETTY_FUNCTION__ << ": There are " << warnings.size() << " errors." << "\n";
		for(const auto & warn : warnings){
			ss << warn << std::endl;
		}
		throw std::runtime_error{ss.str()};
	}
	std::stringstream errorStream;
	errorStream << __PRETTY_FUNCTION__ << ": error\n";
	bool failed = false;
	std::regex pat{"(^MID)([0-9]+$)"};
	std::smatch match;
	for (const auto & row : finalSamps) {
		std::string target = row[0];
		std::string sample = row[1];
		VecStr reps = getSubVector(row, 2);
		if(pars_.byIndex){
			for(const auto & rep : reps){
				if(!std::regex_match(rep, match, pat)){
					errorStream << "Error for " << sample << " in " << target << "rep name needs begins with MID and end with a number" << "\n";
					errorStream << "Failed: " << bib::bashCT::red << rep << bib::bashCT::reset << "\n";
					failed = true;
				}
			}
		}
		for (const auto & rep : reps) {
			for (const auto & tar : samples_) {
				if(tar.first != target){
					continue;
				}
				for (const auto & samp : tar.second.samples_) {
					if (bib::in(rep, samp.second.reps_)) {
						errorStream << __PRETTY_FUNCTION__ << " : Error for " << sample << " in " << target << ", already have " << rep
								<< " in " << samp.first << "\n";
					}
				}
			}
		}
		auto search = samples_.find(target);
		if(samples_.end() == search){
			samples_.emplace(target, Samples(target));
		}
		samples_.at(target).addSample(sample, reps);
	}
	if(failed){
		throw std::runtime_error{errorStream.str()};
	}
}


void TarAmpAnalysisSetup::addRefSeqs(const bfs::path & refSeqsDir){
	/**@todo add ref seq functions and checks*/
	auto fastaFiles = bib::files::listAllFiles(refSeqsDir.string(), false, {
			std::regex { R"(.*\.fasta$)" } });
	VecStr found;
	auto tars = getTargets();
	for (const auto & ff : fastaFiles) {
		auto tarName = bfs::basename(ff.first);
		found.emplace_back(tarName);
		if (!bib::in(tarName, tars)) {
			forRefSeqs_.notMatching_.emplace_back(tarName);
		} else {
			auto inOpts = SeqIOOptions::genFastaIn(ff.first.string(), false);
			SeqInput reader(inOpts);
			auto seqs = reader.readAllReads<seqInfo>();
			if (seqs.size() == 1) {
				idsMids_->targets_.at(tarName).addSingleRef(seqs.front());
			} else {
				idsMids_->targets_.at(tarName).addMultileRef(seqs);
			}
		}
	}
	for (const auto & tar : tars) {
		if (!bib::in(tar, found)) {
			forRefSeqs_.missing_.emplace_back(tar);
		}
	}
}


void TarAmpAnalysisSetup::addLenCutOffs(const bfs::path & lenCutOffsFnp){
	auto multipleLenCutOffs = PrimersAndMids::readInLenCutOffs(lenCutOffsFnp);
	auto targets = getTargets();
	for(const auto & cutOff : multipleLenCutOffs){
		if(!bib::in(cutOff.first, targets)){
			forLenCutOffs_.notMatching_.emplace_back(cutOff.first);
		}
	}
	for (const auto & tar : idsMids_->getTargets()) {
		if (!bib::in(tar, multipleLenCutOffs)) {
			forLenCutOffs_.missing_.emplace_back(tar);
		} else {
			idsMids_->targets_.at(tar).addLenCutOff(
					multipleLenCutOffs.at(tar).minLenChecker_.minLen_,
					multipleLenCutOffs.at(tar).maxLenChecker_.maxLen_);
		}
	}
}


std::vector<VecStr> TarAmpAnalysisSetup::getTarCombos() const{
	std::unordered_map<std::string, VecStr> targetsForReps;
	if (pars_.byIndex) {
		targetsForReps = indexToTars_;
	} else {
		for (const auto & tars : samples_) {
			for (const auto & rep : tars.second.getReps()) {
				targetsForReps[rep].emplace_back(tars.first);
			}
		}
	}

	std::vector<VecStr> tarCombos;
	for (auto & sampTars : targetsForReps) {
		bib::sort(sampTars.second);
		if (!bib::in(sampTars.second, tarCombos)) {
			tarCombos.emplace_back(sampTars.second);
		}
	}
	return tarCombos;
}

void TarAmpAnalysisSetup::writeOutIdFiles() const{
	//now write id files
	std::vector<VecStr> tarCombos = getTarCombos();

	for (const auto & tarCombo : tarCombos) {
		auto collapse = bib::conToStr(tarCombo, "_");
		auto refs = idsMids_->getRefSeqs(tarCombo);
		auto lens = idsMids_->genLenCutOffs(tarCombo);
		if (!lens.empty()) {
			auto lensOutOpts = TableIOOpts::genTabFileOut(
					bib::files::make_path(idsDir_,
							collapse + "_lenCutOffs.tab.txt"));
			lens.outPutContents(lensOutOpts);
		}
		if (!refs.empty()) {
			SeqOutput::write(refs,
					SeqIOOptions::genFastaOut(
							bib::files::make_path(idsDir_, collapse).string()));
		}
		idsMids_->writeIdFile(
				OutOptions(
						bib::files::make_path(idsDir_, collapse + ".id.txt")),
				tarCombo);
	}
}

VecStr TarAmpAnalysisSetup::getExpectantInputNames()const{
	VecStr ret;
	if(pars_.byIndex){
		ret = bib::getVecOfMapKeys(samples_);
	}else{
		ret = getReps();
	}
	return ret;
}


void TarAmpAnalysisSetup::setUpPopClusteringDirs(bool verbose) const {
	auto tars = getTargets();
	if (pars_.byIndex) {
		auto topPopDir = bib::files::makeDir(dir_.string(),
				bib::files::MkdirPar("popClustering"));
		for (const auto & tar : tars) {
			setUpSampleDirs(
					bib::files::make_path(infoDir_, "sampNames.tab.txt").string(),
					bib::files::make_path(topPopDir, tar).string(), false, verbose);
		}
	} else {
		setUpSampleDirs(
							bib::files::make_path(infoDir_, "sampNames.tab.txt").string(),
							bib::files::make_path(dir_, "popClustering").string(), true, verbose);
	}
}


}  // namespace bibseq

