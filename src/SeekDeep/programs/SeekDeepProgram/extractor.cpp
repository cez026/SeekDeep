//
// SeekDeep - A library for analyzing amplicon sequence data
// Copyright (C) 2012-2016 Nicholas Hathaway <nicholas.hathaway@umassmed.edu>,
// Jeffrey Bailey <Jeffrey.Bailey@umassmed.edu>
//
// This file is part of SeekDeep.
//
// SeekDeep is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// SeekDeep is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with SeekDeep.  If not, see <http://www.gnu.org/licenses/>.
//
//
//  main.cpp
//  SeekDeep
//
//  Created by Nicholas Hathaway on 8/11/13.
//  Copyright (c) 2013 Nicholas Hathaway. All rights reserved.
//
#include "SeekDeepRunner.hpp"

namespace bibseq {



int SeekDeepRunner::extractor(const bib::progutils::CmdArgs & inputCommands) {
	SeekDeepSetUp setUp(inputCommands);
	extractorPars pars;
	setUp.setUpExtractor(pars);

	uint32_t readsNotMatchedToBarcode = 0;
	if (pars.checkingQCheck) {
		std::cout << "Quality Check: " << pars.qualCheck << "\n";
		std::cout << "Quality Check Cut Off: " << pars.qualCheckCutOff << "\n";
		std::cout << "Q" << pars.qualCheck << ">" << pars.qualCheckCutOff << "\n";
	} else {
		std::cout << "Quality Window Length: " << pars.qualityWindowLength << "\n";
		std::cout << "Quality Window Step: " << pars.qualityWindowStep << "\n";
		std::cout << "Quality Window Threshold: " << pars.qualityWindowThres
				<< "\n";
	}
	// run log
	setUp.startARunLog(setUp.pars_.directoryName_);
	// parameter file
	setUp.writeParametersFile(setUp.pars_.directoryName_ + "parametersUsed.txt", false,
			false);
	readObject compareObject = readObject(seqInfo("Compare", pars.compareSeq));
	table primerTable = seqUtil::readPrimers(pars.idFilename, pars.idFileDelim,
			false);
	int midSize = 10;
	table mids = seqUtil::readBarcodes(pars.idFilename, pars.idFileDelim,
			midSize);
	std::unique_ptr<MidDeterminator> determinator;
	if (pars.multiplex) {
		determinator = std::make_unique<MidDeterminator>(mids);
	}
	if(setUp.pars_.debug_){
		primerTable.outPutContentOrganized(std::cout);
	}
	PrimerDeterminator pDetermine(primerTable);

	std::string tcagLower = "tcag";
	std::string tcagUpper = "TCAG";
	auto checkForTcag = [&tcagLower, &tcagUpper](const std::string & seq)->bool {
		return std::equal(tcagLower.begin(), tcagLower.end(), seq.begin()) ||
		std::equal(tcagUpper.begin(), tcagUpper.end(), seq.begin());
	};
	// make some directories for outputs
	std::string unfilteredReadsDir = bib::files::makeDir(
			setUp.pars_.directoryName_,
			bib::files::MkdirPar("unfilteredReads", false));
	std::string unfilteredByBarcodesDir = bib::files::makeDir(unfilteredReadsDir,
			bib::files::MkdirPar("byBarcodes", false));
	std::string unfilteredByBarcodesFlowDir = bib::files::makeDir(
			unfilteredReadsDir, bib::files::MkdirPar("flowsByBarcodes", false));
	std::string unfilteredByPrimersDir = bib::files::makeDir(unfilteredReadsDir,
			bib::files::MkdirPar("byPrimers", false));
	std::string filteredOffDir = bib::files::makeDir(setUp.pars_.directoryName_,
			bib::files::MkdirPar("filteredOff", false));
	std::string badDir = bib::files::makeDir(filteredOffDir,
			bib::files::MkdirPar("bad", false));
	std::string unrecognizedPrimerDir = bib::files::makeDir(filteredOffDir,
			bib::files::MkdirPar("unrecognizedPrimer", false));
	std::string contaminationDir = "";
	if (pars.screenForPossibleContamination) {
		contaminationDir = bib::files::makeDir(filteredOffDir,
				bib::files::MkdirPar("contamination", false));
	}
	readObject read;
	// read in reads and remove lower case bases indicating tech low quality like
	// tags and such
	std::cout << "Reading in reads:" << std::endl;
	SeqIO reader(setUp.pars_.ioOptions_);
	reader.openIn();
	auto smallOpts = setUp.pars_.ioOptions_;
	smallOpts.out_.outFilename_ = badDir + "smallFragments";
	SeqIO smallFragMentOut(smallOpts);
	smallFragMentOut.openOut();
	auto startsWtihBadQualOpts = setUp.pars_.ioOptions_;
	startsWtihBadQualOpts.out_.outFilename_ = badDir + "startsWtihBadQual";
	SeqOutput startsWtihBadQualOut(startsWtihBadQualOpts);

	uint32_t smallFragmentCount = 0;
	uint32_t startsWithBadQualCount = 0;
	uint64_t maxReadSize = 0;
	uint32_t count = 0;
	MultiSeqIO readerOuts;
	std::map<std::string, std::pair<uint32_t, uint32_t>> counts;
	if (pars.multiplex && pars.barcodeErrors > 0) {
		if (setUp.pars_.debug_) {
			std::cout << "Allowing " << pars.barcodeErrors << " errors in barcode"
					<< std::endl;
		}
	}
	if (pars.multiplex) {
		for (const auto & mid : determinator->mids_) {
			auto midOpts = setUp.pars_.ioOptions_;
			midOpts.out_.outFilename_ = unfilteredByBarcodesDir + mid.first;
			if (setUp.pars_.debug_) {
				std::cout << "Inserting: " << mid.first << std::endl;
			}
			readerOuts.addReader(mid.first, midOpts);
			if (pars.mothurExtract || pars.pyroExtract) {
				auto flowExtractOutOpts = midOpts;
				flowExtractOutOpts.out_.outFilename_ = unfilteredByBarcodesFlowDir + mid.first + "_temp";
				flowExtractOutOpts.outFormat_ = SeqIOOptions::outFormats::FLOW;
				flowExtractOutOpts.out_.outExtention_ = ".dat";
				readerOuts.addReader(mid.first + "flow", flowExtractOutOpts);
			}
		}
	} else {
		auto midOpts = setUp.pars_.ioOptions_;
		midOpts.out_.outFilename_ = unfilteredByBarcodesDir + "all";
		if (setUp.pars_.debug_) {
			std::cout << "Inserting: " << "all" << std::endl;
		}
		readerOuts.addReader("all", midOpts);
		if (pars.mothurExtract || pars.pyroExtract) {
			auto flowExtractOutOpts = midOpts;
			flowExtractOutOpts.out_.outFilename_ = unfilteredByBarcodesFlowDir + "all" + "_temp";
			flowExtractOutOpts.outFormat_ = SeqIOOptions::outFormats::FLOW;
			flowExtractOutOpts.out_.outExtention_ = ".dat";
			readerOuts.addReader(std::string("all") + "flow", flowExtractOutOpts);
		}
	}

	if (pars.multiplex) {
		auto midOpts = setUp.pars_.ioOptions_;
		midOpts.out_.outFilename_ = badDir + "unrecognizedBarcode";
		if (setUp.pars_.debug_){
			std::cout << "Inserting: " << "unrecognized" << std::endl;
		}
		readerOuts.addReader("unrecognized", midOpts);
	}

	std::cout << bib::bashCT::boldGreen("Extracting on MIDs") << std::endl;
	while (reader.readNextRead(read)) {
		++count;
		if (count % 50 == 0) {
			std::cout << "\r" << count ;
			std::cout.flush();
		}

		if (pars.HMP) {
			subStrToUpper(read.seqBase_.seq_, 4, pars.primerLen);
		}
		if (pars.trimTcag) {
			if (checkForTcag(read.seqBase_.seq_)) {
				read.trimFront(4);
			}
		}
		readVec::handelLowerCaseBases(read, setUp.pars_.ioOptions_.lowerCaseBases_);

		//possibly trim reads at low quality
		if(pars.trimAtQual){
			readVecTrimmer::trimAtFirstQualScore(read.seqBase_, pars.trimAtQualCutOff);
			if(0 == len(read)){
				startsWtihBadQualOut.openWrite(read);
				++startsWithBadQualCount;
				continue;
			}
		}

		if (len(read) < pars.smallFragmentCutoff) {
			smallFragMentOut.write(read);
			++smallFragmentCount;
			continue;
		}
		readVec::getMaxLength(read, maxReadSize);

		midPos currentMid;
		if (pars.multiplex) {
			if (pars.barcodeErrors > 0) {
				currentMid = determinator->fullDetermine(read, pars.variableStart,
						pars.variableStop, pars.checkComplement, pars.barcodesBothEnds,
						pars.barcodeErrors);
			} else {
				currentMid = determinator->fullDetermine(read, pars.variableStart,
						pars.variableStop, pars.checkComplement, pars.barcodesBothEnds);
			}
		} else {
			currentMid = midPos("all", 0, 0);
		}

		if (!currentMid) {
			++readsNotMatchedToBarcode;
		}

		if (read.seqBase_.name_.find("_Comp") != std::string::npos) {
			++counts[currentMid.midName_].second;
		} else {
			++counts[currentMid.midName_].first;
		}
		/**@todo need to reorient the reads here before outputing if that's needed*/
		readerOuts.openWrite(currentMid.midName_, read);
		if (currentMid.midName_ != "unrecognized" && (pars.mothurExtract || pars.pyroExtract)) {
			readerOuts.openWriteFlow(currentMid.midName_ + "flow", *reader.in_.lastSffRead_);
		}
	}
	std::cout << std::endl;
	//close mid outs;
	readerOuts.closeOutAll();

	if (setUp.pars_.debug_) {
		table midCounts { VecStr { "MidName", "For", "Rev" } };
		for (const auto & mCount : counts) {
			midCounts.content_.emplace_back(
					toVecStr(mCount.first, mCount.second.first, mCount.second.second));
		}
		midCounts.outPutContentOrganized(std::cout);
	}

	std::ofstream renameKeyFile;
	if (pars.rename) {
		openTextFile(renameKeyFile, setUp.pars_.directoryName_ + "renameKey.tab.txt",
				".tab.txt", false, false);
		renameKeyFile << "originalName\tnewName\n";
	}

	if (pars.noForwardPrimer) {
		if (primerTable.content_.size() > 1) {
			std::cerr
					<< "Error, if noForwardPrimer is turned on can only supply one gene name, curently have: "
					<< primerTable.content_.size() << std::endl;
			std::cerr << bib::conToStr(primerTable.getColumn("geneName"), ",")
					<< std::endl;
			exit(1);
		}
	}
	auto barcodeFiles = bib::files::listAllFiles(unfilteredByBarcodesDir, false,
			VecStr { });
	std::unique_ptr<ReadCheckerOnKmerComp> compareInfo;
	std::unique_ptr<ReadCheckerOnKmerComp> compareInfoRev;
	std::map<std::string, ReadCheckerOnKmerComp> compareInfos;
	std::map<std::string, ReadCheckerOnKmerComp> compareInfosRev;
	struct lenCutOffs {
		lenCutOffs(uint32_t minLen, uint32_t maxLen, bool mark = true):
			minLenChecker_(ReadCheckerLenAbove(minLen, mark)),
			maxLenChecker_(ReadCheckerLenBelow(maxLen, mark)){
		}
		ReadCheckerLenAbove minLenChecker_;
		ReadCheckerLenBelow maxLenChecker_;
	};
	lenCutOffs seqLenCutOffs(pars.minLen, pars.maxLength);
	std::map<std::string, lenCutOffs> multipleLenCutOffs;
	if (pars.multipleTargets && pars.multipleLenCutOffFilename != "") {
		table lenCutTab = table(pars.multipleLenCutOffFilename, "whitespace", true);
		bib::for_each(lenCutTab.columnNames_,
				[](std::string & str) {stringToLower(str);});
		lenCutTab.setColNamePositions();
		if (!bib::in(std::string("target"), lenCutTab.columnNames_)
				|| !bib::in(std::string("minlen"), lenCutTab.columnNames_)
				|| !bib::in(std::string("maxlen"), lenCutTab.columnNames_)) {
			std::stringstream ss;
			ss << "need to have columns " << "target,minlen, and maxlen"
					<< " when reading in a table for multiple cut off lengths"
					<< std::endl;
			ss << "only have " << vectorToString(lenCutTab.columnNames_, ",")
					<< std::endl;
			throw std::runtime_error { bib::bashCT::boldRed(ss.str()) };
		}
		for (const auto & row : lenCutTab.content_) {
			multipleLenCutOffs.emplace(row[lenCutTab.getColPos("target")],
					lenCutOffs { bib::lexical_cast<uint32_t>(
							row[lenCutTab.getColPos("minlen")]), bib::lexical_cast<uint32_t>(
							row[lenCutTab.getColPos("maxlen")]) });
		}
	}
	std::unique_ptr<ReadChecker> qualChecker;
	if (pars.checkingQCheck) {
		qualChecker = std::make_unique<ReadCheckerQualCheck>(pars.qualCheck,
				pars.qualCheckCutOff, true);
	} else {
		if (pars.qualWindowTrim) {
			qualChecker = std::make_unique<ReadCheckerOnQualityWindowTrim>(pars.qualityWindowLength, pars.qualityWindowStep,
					pars.qualityWindowThres, pars.minLen, true);
		} else {
			qualChecker = std::make_unique<ReadCheckerOnQualityWindow>(pars.qualityWindowLength, pars.qualityWindowStep,
					pars.qualityWindowThres, true);
		}
	}
	ReadCheckerOnSeqContaining nChecker("N", pars.numberOfNs, true);
	if (pars.screenForPossibleContamination) {
		readVec::getMaxLength(compareObject, maxReadSize);

		compareInfo = std::make_unique<ReadCheckerOnKmerComp>(kmerInfo(compareObject.seqBase_.seq_,
				pars.contaminationKLen, false),pars.contaminationKLen, pars.kmerCutOff, true);
		auto rev = compareObject;
		rev.seqBase_.reverseComplementRead(true, true);
		compareInfoRev = std::make_unique<ReadCheckerOnKmerComp>(kmerInfo(rev.seqBase_.seq_,
				pars.contaminationKLen, false),pars.contaminationKLen, pars.kmerCutOff, true);
		if(pars.contaminationMutlipleCompare){
			if(bfs::is_regular_file(pars.compareSeqFilename)){
				if(setUp.pars_.verbose_){
					std::cout << pars.compareSeqFilename << std::endl;
				}
				SeqIOOptions conTamOpts;
				conTamOpts.inFormat_ = SeqIOOptions::getInFormat(bib::files::getExtension(pars.compareSeqFilename));
				conTamOpts.firstName_ = pars.compareSeqFilename;
				SeqInput readerCon(conTamOpts);
				readerCon.openIn();
				auto reads = readerCon.readAllReads<readObject>();
				if (setUp.pars_.verbose_) {
					std::cout << "reads" << std::endl;
					std::cout << vectorToString(readVec::getNames(reads), ",")
							<< std::endl;
				}

				for (const auto & read : reads) {
					readVec::getMaxLength(read, maxReadSize);
					compareInfos.emplace(read.seqBase_.name_, ReadCheckerOnKmerComp(kmerInfo(read.seqBase_.seq_,
							pars.contaminationKLen, false),pars.contaminationKLen, pars.kmerCutOff, false));
					auto rev = read;
					rev.seqBase_.reverseComplementRead(true, true);
					compareInfosRev.emplace(read.seqBase_.name_, ReadCheckerOnKmerComp(kmerInfo(rev.seqBase_.seq_,
										pars.contaminationKLen, false),pars.contaminationKLen, pars.kmerCutOff, false));
				}
			}else{
				compareInfos.emplace(read.seqBase_.name_, ReadCheckerOnKmerComp(kmerInfo(compareObject.seqBase_.seq_,
						pars.contaminationKLen, false),pars.contaminationKLen, pars.kmerCutOff, true));
				compareInfosRev.emplace(read.seqBase_.name_, ReadCheckerOnKmerComp(kmerInfo(rev.seqBase_.seq_,
						pars.contaminationKLen, false),pars.contaminationKLen, pars.kmerCutOff, true));
			}
		}
	}

	if (pars.multipleTargets && pars.screenForPossibleContamination) {
		std::cout << pars.compareSeqFilename << std::endl;
		SeqIOOptions conTamOpts;
		conTamOpts.inFormat_ = SeqIOOptions::getInFormat(bib::files::getExtension(pars.compareSeqFilename));
		conTamOpts.firstName_ = pars.compareSeqFilename;
		SeqInput readerCon(conTamOpts);
		readerCon.openIn();
		auto reads = readerCon.readAllReads<readObject>();
		std::cout << "reads" << std::endl;
		std::cout << vectorToString(readVec::getNames(reads), ",") << std::endl;
		for (const auto & read : reads) {
			readVec::getMaxLength(read, maxReadSize);
			compareInfos.emplace(read.seqBase_.name_, ReadCheckerOnKmerComp(kmerInfo(read.seqBase_.seq_,
					pars.contaminationKLen, false),pars.contaminationKLen, pars.kmerCutOff, true));
			auto rev = read;
			rev.seqBase_.reverseComplementRead(true, true);
			compareInfosRev.emplace(read.seqBase_.name_, ReadCheckerOnKmerComp(kmerInfo(rev.seqBase_.seq_,
								pars.contaminationKLen, false),pars.contaminationKLen, pars.kmerCutOff, true));
		}
	}


	// create aligner for primer identification
	if(setUp.pars_.debug_){
		std::cout << bib::bashCT::boldGreen("Creating Scoring Matrix: Start") << std::endl;
	}
	auto scoreMatrix = substituteMatrix::createDegenScoreMatrixNoNInRef(
			setUp.pars_.generalMatch_, setUp.pars_.generalMismatch_);
	gapScoringParameters gapPars(setUp.pars_.gapInfo_);
	KmerMaps emptyMaps;
	bool countEndGaps = true;
	if(setUp.pars_.debug_){
		std::cout << bib::bashCT::boldRed("Creating Scoring Matrix: Stop") << std::endl;
		std::cout << bib::bashCT::boldGreen("Determining Max Read Size: Start") << std::endl;
	}
	//to avoid allocating an extremely large aligner matrix;
	if(maxReadSize > 1000){
		auto maxPrimerSize = pDetermine.getMaxPrimerSize();
		if(setUp.pars_.debug_){
			std::cout << bib::bashCT::boldBlack("maxPrimerSize: ") << maxPrimerSize << std::endl;
		}
		maxReadSize =  maxPrimerSize * 4 + pars.variableStop;
	}
	if(setUp.pars_.debug_){
		std::cout << bib::bashCT::boldRed("Determining Max Read Size: Stop") << std::endl;
		std::cout << bib::bashCT::boldGreen("Creating Aligner: Start") << std::endl;
		std::cout << bib::bashCT::boldBlack("max read size: " ) << maxReadSize << std::endl;
	}

	aligner alignObj = aligner(maxReadSize, gapPars, scoreMatrix, emptyMaps,
			setUp.pars_.qScorePars_, countEndGaps, false);
	if(setUp.pars_.debug_){
		std::cout << bib::bashCT::boldRed("Creating Aligner: Stop") << std::endl;
		std::cout << bib::bashCT::boldGreen("Reading In Previous Alignments: Start") << std::endl;
	}
	alignObj.processAlnInfoInput(setUp.pars_.alnInfoDirName_);
	if(setUp.pars_.debug_){
		std::cout << bib::bashCT::boldRed("Reading In Previous Alignments: Stop") << std::endl;
		std::cout << bib::bashCT::boldGreen("Creating Extractor Stats: Start") << std::endl;
	}
	std::string smallDir = "";
	if (pars.filterOffSmallReadCounts) {
		smallDir = bib::files::makeDir(setUp.pars_.directoryName_, bib::files::MkdirPar("smallReadCounts", false));
	}

	ExtractionStator stats = ExtractionStator(count, readsNotMatchedToBarcode,
			smallFragmentCount);
	if(setUp.pars_.debug_){
		std::cout << bib::bashCT::boldRed("Creating Extractor Stats: Stop") << std::endl;
	}
	std::map<std::string, uint32_t> goodCounts;

	for (const auto & f : barcodeFiles) {
		auto barcodeName = bib::files::getFileName(f.first.string());
		if ((counts[barcodeName].first + counts[barcodeName].second) == 0
				&& pars.multiplex) {
			//no reads extracted for barcode so skip filtering step
			continue;
		}

		if (pars.filterOffSmallReadCounts
				&& (counts[barcodeName].first + counts[barcodeName].second)
						<= pars.smallExtractReadCount) {
			auto barcodeOpts = setUp.pars_.ioOptions_;
			barcodeOpts.firstName_ = f.first.string();
			barcodeOpts.inFormat_ = SeqIOOptions::getInFormat(bib::files::getExtension(f.first.string()));
			barcodeOpts.out_.outFilename_ = smallDir + barcodeName;
			SeqIO barcodeIn(barcodeOpts);
			barcodeIn.openIn();
			readObject read;
			while (barcodeIn.readNextRead(read)) {
				barcodeIn.openWrite(read);
			}
			continue;
		}

		if (pars.multiplex) {
			std::cout
					<< bib::bashCT::boldGreen("Filtering on barcode: " + barcodeName)
					<< std::endl;
		} else {
			std::cout << bib::bashCT::boldGreen("Filtering") << std::endl;
		}
		auto barcodeOpts = setUp.pars_.ioOptions_;
		barcodeOpts.firstName_ = f.first.string();
		barcodeOpts.inFormat_ = SeqIOOptions::getInFormat(
				bib::files::getExtension(f.first.string()));
		SeqIO barcodeIn(barcodeOpts);
		barcodeIn.openIn();

		//create outputs
		MultiSeqIO midReaderOuts;
		auto unrecogPrimerOutOpts = setUp.pars_.ioOptions_;
		unrecogPrimerOutOpts.out_.outFilename_ = unrecognizedPrimerDir
				+ barcodeName;
		midReaderOuts.addReader("unrecognized", unrecogPrimerOutOpts);

		for (const auto & primerName : getVectorOfMapKeys(pDetermine.primers_)) {
			std::string fullname = primerName;
			if (pars.multiplex) {
				fullname += barcodeName;
			} else if (pars.sampleName != "") {
				fullname += pars.sampleName;
			}
			//bad out
			auto badDirOutOpts = setUp.pars_.ioOptions_;
			badDirOutOpts.out_.outFilename_ = badDir + fullname;
			midReaderOuts.addReader(fullname + "bad", badDirOutOpts);
			//good out
			auto goodDirOutOpts = setUp.pars_.ioOptions_;
			goodDirOutOpts.out_.outFilename_ = setUp.pars_.directoryName_ + fullname;
			midReaderOuts.addReader(fullname + "good", goodDirOutOpts);
			//contamination out
			if (pars.screenForPossibleContamination) {
				auto contamOutOpts = setUp.pars_.ioOptions_;
				contamOutOpts.out_.outFilename_ = contaminationDir + fullname;
				midReaderOuts.addReader(fullname + "contamination", contamOutOpts);
			}
			if (pars.mothurExtract || pars.pyroExtract) {
				auto flowExtractOutOpts = setUp.pars_.ioOptions_;
				flowExtractOutOpts.out_.outFilename_ = setUp.pars_.directoryName_ + fullname + "_temp";
				flowExtractOutOpts.outFormat_ = SeqIOOptions::outFormats::FLOW;
				flowExtractOutOpts.out_.outExtention_ = ".dat";
				midReaderOuts.addReader(fullname + "flow", flowExtractOutOpts);
			}
		}

		uint32_t barcodeCount = 1;
		bib::ProgressBar pbar(
				counts[barcodeName].first + counts[barcodeName].second);
		pbar.progColors_ = pbar.RdYlGn_;
		std::string readFlows = "";
		std::ifstream inFlowFile;
		if(pars.mothurExtract || pars.pyroExtract){
			inFlowFile.open(unfilteredByBarcodesFlowDir + barcodeName + "_temp.dat");
		}



		while (barcodeIn.readNextRead(read)) {
			//std::cout << barcodeCount << std::endl;
			std::getline(inFlowFile, readFlows);
			pbar.outputProg(std::cout, barcodeCount, true);
			++barcodeCount;
			//filter on primers
			//forward
			std::string primerName = "";
			bool foundInReverse = false;
			if (pars.noForwardPrimer) {
				primerName = primerTable.content_.front()[primerTable.getColPos(
						"geneName")];
			} else {
				if (pars.multiplex) {
					primerName = pDetermine.determineForwardPrimer(read, 0, alignObj,
							pars.fPrimerErrors, !pars.forwardPrimerToUpperCase);
					if (primerName == "unrecognized" && pars.checkComplement) {
						primerName = pDetermine.determineWithReversePrimer(read, 0,
								alignObj, pars.fPrimerErrors, !pars.forwardPrimerToUpperCase);
						if (read.seqBase_.on_) {
							foundInReverse = true;
						}
					}
				} else {
					uint32_t start = 0;
					if (pars.variableStart) {
						start = pars.variableStop;
					}
					//std::cout << "Determining primer" << std::endl;

					primerName = pDetermine.determineForwardPrimer(read, start, alignObj,
							pars.fPrimerErrors, !pars.forwardPrimerToUpperCase);
					if (primerName == "unrecognized" && pars.checkComplement) {
						primerName = pDetermine.determineWithReversePrimer(read, start,
								alignObj, pars.fPrimerErrors, !pars.forwardPrimerToUpperCase);
						if (read.seqBase_.on_) {
							foundInReverse = true;
						}
					}
				}

				if (!read.seqBase_.on_) {
					stats.increaseFailedForward(barcodeName, read.seqBase_.name_);
					midReaderOuts.openWrite("unrecognized", read);
					continue;
				}
			}
			std::string fullname = primerName;
			if (pars.multiplex) {
				fullname += barcodeName;
			} else if (pars.sampleName != "") {
				fullname += pars.sampleName;
			}



			//look for possible contamination
			if (pars.screenForPossibleContamination) {
				if (pars.multipleTargets) {
					if (compareInfos.find(primerName) != compareInfos.end()) {
						if (foundInReverse) {
							compareInfosRev.at(primerName).checkRead(read.seqBase_);
						} else {
							compareInfos.at(primerName).checkRead(read.seqBase_);
						}

						if (!read.seqBase_.on_) {
							stats.increaseCounts(fullname, read.seqBase_.name_,
									ExtractionStator::extractCase::CONTAMINATION);
							midReaderOuts.openWrite(fullname + "contamination", read);
							continue;
						}
					} else {
						std::stringstream ss;
						ss
								<< "Error in screening for contamination, multiple targets turned on but no contamination found for "
								<< primerName << std::endl;
						ss << "Options are: "
								<< vectorToString(getVectorOfMapKeys(compareInfos), ",")
								<< std::endl;
						throw std::runtime_error{ss.str()};
					}
				} else {
					if(pars.contaminationMutlipleCompare){
						//if it passes at least one of the multiple compares given in compareSeq filename
						if (foundInReverse) {
							for(const auto & compares : compareInfosRev){
								if(compares.second.checkRead(read.seqBase_)){
									break;
								}
							}
						}else{
							for(const auto & compares : compareInfos){
								if(compares.second.checkRead(read.seqBase_)){
									break;
								}
							}
						}
					}else{
						if (foundInReverse) {
							compareInfoRev->checkRead(read.seqBase_);
						} else {
							compareInfo->checkRead(read.seqBase_);
						}
					}
					if (!read.seqBase_.on_) {
						stats.increaseCounts(fullname, read.seqBase_.name_,
								ExtractionStator::extractCase::CONTAMINATION);
						midReaderOuts.openWrite(fullname + "contamination", read);
						continue;
					}
				}
			}
			//min len
			if (pars.multipleTargets) {
				if (bib::in(primerName, multipleLenCutOffs)) {
					multipleLenCutOffs.at(primerName).minLenChecker_.checkRead(read.seqBase_);
				} else {
					seqLenCutOffs.minLenChecker_.checkRead(read.seqBase_);
				}
			} else {
				seqLenCutOffs.minLenChecker_.checkRead(read.seqBase_);
			}
			if (!read.seqBase_.on_) {
				stats.increaseCounts(fullname, read.seqBase_.name_,
						ExtractionStator::extractCase::MINLENBAD);
				midReaderOuts.openWrite(fullname + "bad", read);
				continue;
			}

			//reverse
			if (!pars.noReversePrimer) {
				if (foundInReverse) {
					pDetermine.checkForForwardPrimerInRev(read, primerName, alignObj,
							pars.rPrimerErrors, !pars.reversePrimerToUpperCase,
							pars.variableStop, false);
				} else {
					pDetermine.checkForReversePrimer(read, primerName, alignObj,
							pars.rPrimerErrors, !pars.reversePrimerToUpperCase,
							pars.variableStop, false);
				}

				if (!read.seqBase_.on_) {
					stats.increaseCounts(fullname, read.seqBase_.name_,
							ExtractionStator::extractCase::BADREVERSE);
					read.seqBase_.name_.append("_badReverse");
					midReaderOuts.openWrite(fullname + "bad", read);
					continue;
				}
			}
			//min len again becuase the reverse primer search trims to the reverse primer so it could be short again
			if (pars.multipleTargets) {
				if (bib::in(primerName, multipleLenCutOffs)) {
					multipleLenCutOffs.at(primerName).minLenChecker_.checkRead(
							read.seqBase_);
				} else {
					seqLenCutOffs.minLenChecker_.checkRead(read.seqBase_);
				}
			} else {
				seqLenCutOffs.minLenChecker_.checkRead(read.seqBase_);
			}
			if (!read.seqBase_.on_) {
				stats.increaseCounts(fullname, read.seqBase_.name_,
						ExtractionStator::extractCase::MINLENBAD);
				midReaderOuts.openWrite(fullname + "bad", read);
				continue;
			}

			//if found in the reverse direction need to re-orient now
			if (foundInReverse) {
				read.seqBase_.reverseComplementRead(true, true);
			}

			//contains n
			nChecker.checkRead(read.seqBase_);
			if (!read.seqBase_.on_) {
				stats.increaseCounts(fullname, read.seqBase_.name_,
						ExtractionStator::extractCase::CONTAINSNS);
				midReaderOuts.openWrite(fullname + "bad", read);
				continue;
			}

			//max len
			if (pars.multipleTargets) {
				if (bib::in(primerName, multipleLenCutOffs)) {
					multipleLenCutOffs.at(primerName).maxLenChecker_.checkRead(read.seqBase_);
				} else {
					seqLenCutOffs.maxLenChecker_.checkRead(read.seqBase_);
				}
			} else {
				seqLenCutOffs.maxLenChecker_.checkRead(read.seqBase_);
			}
			if (!read.seqBase_.on_) {
				stats.increaseCounts(fullname, read.seqBase_.name_,
						ExtractionStator::extractCase::MAXLENBAD);
				midReaderOuts.openWrite(fullname + "bad", read);
				continue;
			}


			//quality
			qualChecker->checkRead(read.seqBase_);

			if (!read.seqBase_.on_) {
				stats.increaseCounts(fullname, read.seqBase_.name_,
						ExtractionStator::extractCase::QUALITYFAILED);
				midReaderOuts.openWrite(fullname + "bad", read);
				continue;
			}

			if (read.seqBase_.on_) {
				stats.increaseCounts(fullname, read.seqBase_.name_,
						ExtractionStator::extractCase::GOOD);
				if (pars.rename) {
					std::string oldName = bib::replaceString(read.seqBase_.name_, "_Comp", "");
					read.seqBase_.name_ = fullname + "."
							+ leftPadNumStr(goodCounts[fullname],
									counts[barcodeName].first + counts[barcodeName].second);
					if (bib::containsSubString(oldName, "_Comp")) {
						read.seqBase_.name_.append("_Comp");
					}
					renameKeyFile << oldName << "\t" << read.seqBase_.name_ << "\n";
				}
				midReaderOuts.openWrite(fullname + "good", read);
				if(pars.mothurExtract || pars.pyroExtract){
					midReaderOuts.openWrite(fullname + "flow", readFlows);
				}
				++goodCounts[fullname];
			}
		}
		std::cout << std::endl;
	}
	if(pars.mothurExtract || pars.pyroExtract){
		if(pars.mothurExtract){
			for(const auto & name : goodCounts){
				std::ofstream mothurOut;
				openTextFile(mothurOut,setUp.pars_.directoryName_ + name.first,".flow",false, true);
				uint32_t mothurExtractFlowNum = 800;
				if (pars.maxFlowCutoff == 720) {
					mothurExtractFlowNum = 800;
				} else if (pars.maxFlowCutoff <= 400) {
					mothurExtractFlowNum = 400;
				}
				mothurOut << mothurExtractFlowNum << std::endl;
				std::ifstream flowFile(setUp.pars_.directoryName_ + name.first + "_temp.dat");
				for(std::string line; std::getline(flowFile,line);){
					mothurOut << line << std::endl;
				}
			}
		}
		if(pars.pyroExtract){
			for(const auto & name : goodCounts){
				std::ofstream ampNoiseOut;
				openTextFile(ampNoiseOut,setUp.pars_.directoryName_ + name.first,".dat",false, true);
				ampNoiseOut << name.second << " " << pars.maxFlowCutoff << std::endl;
				std::ifstream flowFile(setUp.pars_.directoryName_ + name.first + "_temp.dat");
				for(std::string line; std::getline(flowFile,line);){
					ampNoiseOut << line << std::endl;
				}
			}
		}
		for(const auto & name : goodCounts){
			bib::files::bfs::remove(setUp.pars_.directoryName_ + name.first + "_temp.dat");
		}
	}
	std::ofstream profileLog;
	openTextFile(profileLog, setUp.pars_.directoryName_ + "extractionProfile.tab.txt",
			".txt", false, false);

	profileLog
			<< "name\ttotalReadsExtracted\tgoodReadsExtracted\tforGood\trevGood\ttotalBadReads\t"
					"badReverse\tcontainsNs\tlen<" << pars.minLen << "\tlen>"
			<< pars.maxLength;
	if (pars.checkingQCheck) {
		profileLog << "\tq" + estd::to_string(pars.qualCheck) + "<"
				<< pars.qualCheckCutOff;
	} else {
		profileLog << "\tbadQualityWindow";
	}
	profileLog << "\tcontamination\n";
	stats.outStatsPerName(profileLog, "\t");
	std::ofstream extractionStatsFile;
	openTextFile(extractionStatsFile,
			setUp.pars_.directoryName_ + "extractionStats.tab.txt", ".txt",
			setUp.pars_.ioOptions_.out_);
	extractionStatsFile
			<< "TotalReads\tReadsNotMatchedBarcodes\tSmallFragments(len<"
			<< pars.smallFragmentCutoff
			<< ")\tfailedForwardPrimer\tfailedQualityFiltering\tused";
	extractionStatsFile << "\tcontamination";
	extractionStatsFile << std::endl;
	stats.outTotalStats(extractionStatsFile, "\t");
	std::ofstream failedForwardFile;
	openTextFile(failedForwardFile, setUp.pars_.directoryName_ + "failedForward.tab.txt",
			".txt", false, false);
	failedForwardFile << "MidName\ttotalFailed\tfailedInFor\tfailedInRev"
			<< std::endl;
	stats.outFailedForwardStats(failedForwardFile, "\t");

	if (!setUp.pars_.debug_) {
		bib::files::rmDirForce(unfilteredReadsDir);
	}
	if (setUp.pars_.writingOutAlnInfo_) {
		setUp.rLog_ << "Number of alignments done" << "\n";
		alignObj.alnHolder_.write(setUp.pars_.outAlnInfoDirName_, setUp.pars_.verbose_);
	}
	setUp.logRunTime(std::cout);
	return 0;
}



}  // namespace bib
