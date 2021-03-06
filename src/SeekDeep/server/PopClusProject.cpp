/*
 * PopClusProject.cpp
 *
 *  Created on: Sep 19, 2016
 *      Author: nick
 */




#include "PopClusProject.hpp"
#include <unordered_map>

namespace bibseq {

PopClusProject::PopClusProject(const Json::Value & configJson) :
		config_(configJson) {
	bib::json::MemberChecker checker(configJson);
	checker.failMemberCheckThrow( { "shortName", "projectName", "mainDir" },
			__PRETTY_FUNCTION__);
	auto coreJsonFnp = bib::files::make_path(configJson["mainDir"],
			"coreInfo.json");
	if (!bfs::exists(coreJsonFnp)) {
		std::stringstream ss;
		ss << __PRETTY_FUNCTION__ << ": Error, project "
				<< configJson["projectName"] << "configuration "
				<< " main directory doesn't contain coreInfo.json file" << "\n";
		ss << coreJsonFnp << " doesn't exist" << "\n";
		throw std::runtime_error { ss.str() };
	}
	collection_ = std::make_unique<collapse::SampleCollapseCollection>(
			bib::json::parseFile(coreJsonFnp.string()));
	shortName_ = config_["shortName"].asString();
	projectName_ = config_["projectName"].asString();

	tabs_.popInfo_ = std::make_unique<TableCache>(TableIOOpts(InOptions(collection_->getPopInfoPath()), "\t", true));
	tabs_.sampInfo_ = std::make_unique<TableCache>(TableIOOpts(InOptions(collection_->getSampInfoPath()), "\t", true));
	tabs_.hapIdTab_ = std::make_unique<TableCache>(TableIOOpts(InOptions(collection_->getHapIdTabPath()), "\t", true));

	//set up group meta data
	if(nullptr != collection_->groupDataPaths_){
		for(const auto & group : collection_->groupDataPaths_->allGroupPaths_){
			topGroupTabs_[group.first] = std::make_unique<TableCache>(TableIOOpts(InOptions(group.second.groupInfoFnp_), "\t", true));
			for(const auto & subGroup : group.second.groupPaths_){
				subGroupTabs_[group.first][subGroup.first].popInfo_ = std::make_unique<TableCache>(TableIOOpts(InOptions(subGroup.second.popFileFnp_), "\t", true));
				subGroupTabs_[group.first][subGroup.first].sampInfo_ = std::make_unique<TableCache>(TableIOOpts(InOptions(subGroup.second.sampFileFnp_), "\t", true));
				subGroupTabs_[group.first][subGroup.first].hapIdTab_ = std::make_unique<TableCache>(TableIOOpts(InOptions(subGroup.second.hapIdTabFnp_), "\t", true));
			}
		}
	}

	auto extractionProfileFnp = bib::files::make_path(configJson["mainDir"].asString(), "extractionInfo", "extractionProfile.tab.txt");
	auto extractionStatsFnp = bib::files::make_path(configJson["mainDir"].asString(), "extractionInfo", "extractionStats.tab.txt");

	if(bfs::exists(extractionProfileFnp)){
		extractionProfileTab_ = std::make_unique<TableCache>(TableIOOpts(InOptions(extractionProfileFnp), "\t", true));
	}

	if(bfs::exists(extractionStatsFnp)){
		extractionStatsTab_ = std::make_unique<TableCache>(TableIOOpts(InOptions(extractionStatsFnp), "\t", true));
	}

}


void PopClusProject::registerSeqFiles(SeqCache & cache) {
	auto popHapFile = collection_->getPopFinalHapsPath().string();
	cache.updateAddCache(shortName_,
			SeqIOOptions(popHapFile,
					SeqIOOptions::getInFormat(bib::files::getExtension(popHapFile)),
					true));
	for(const auto & samp : collection_->popNames_.samples_){
		auto sampHapFile = collection_->getSampleFinalHapsPath(samp).string();
		if(bfs::exists(sampHapFile)){
			cache.updateAddCache(shortName_ + "_" + samp,
					SeqIOOptions(sampHapFile,
							SeqIOOptions::getInFormat(bib::files::getExtension(sampHapFile)),
							true));
		}
	}
}

}  // namespace bibseq
