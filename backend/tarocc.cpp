#include <iostream>
#include <fstream>
#include <memory>
#include <regex>
#include <string>
#include <tuple>
#include <unordered_map>

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/Path.h"

#include "llvm/Support/raw_ostream.h"

#include "nlohmann/json.hpp"

#include "tapa/task.h"

//#define DEBUG

using std::cout;
using std::endl;
using std::make_shared;
using std::regex;
using std::regex_match;
using std::regex_replace;
using std::shared_ptr;
using std::stoi;
using std::string;
using std::tie;
using std::tuple;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

using clang::ASTConsumer;
using clang::ASTContext;
using clang::ASTFrontendAction;
using clang::CompilerInstance;
using clang::FunctionDecl;
using clang::Rewriter;
using clang::Stmt;
using clang::StringRef;
using clang::tooling::ClangTool;
using clang::tooling::CommonOptionsParser;
using clang::tooling::newFrontendActionFactory;

using llvm::raw_string_ostream;
using llvm::cl::NumOccurrencesFlag;
using llvm::cl::OptionCategory;
using llvm::cl::ValueExpected;
using llvm::dyn_cast;

using nlohmann::json;

enum class StreamOpEnum : uint64_t {
	kNotStreamOperation = 0,
	kIsDestructive = 1 << 0,  // Changes FIFO state.
	kIsNonBlocking = 1 << 1,
	kIsBlocking = 1 << 2,
	kIsDefaulted = 1 << 3,
	kIsProducer = 1 << 4,  // Must be performed on the producer side.
	kIsConsumer = 1 << 5,  // Must be performed on the consumer side.
	kNeedPeeking = 1 << 6,
	kNeedEos = 1 << 7,

	kTestEmpty = 1ULL << 32 | kIsConsumer,
	kTryEos = 2ULL << 32 | kIsConsumer | kIsNonBlocking | kNeedPeeking | kNeedEos,
	kBlockingEos =
		3ULL << 32 | kIsConsumer | kIsBlocking | kNeedPeeking | kNeedEos,
	kNonBlockingEos =
		4ULL << 32 | kIsConsumer | kIsNonBlocking | kNeedPeeking | kNeedEos,
	kTryPeek = 5ULL << 32 | kIsConsumer | kIsNonBlocking | kNeedPeeking,
	kNonBlockingPeek = 6ULL << 32 | kIsNonBlocking | kIsConsumer | kNeedPeeking,
	kNonBlockingPeekWithEos =
		7ULL << 32 | kIsNonBlocking | kIsConsumer | kNeedPeeking | kNeedEos,
	kTryRead = 8ULL << 32 | kIsDestructive | kIsNonBlocking | kIsConsumer,
	kBlockingRead = 9ULL << 32 | kIsDestructive | kIsBlocking | kIsConsumer,
	kNonBlockingRead =
		10ULL << 32 | kIsDestructive | kIsNonBlocking | kIsConsumer,
	kNonBlockingDefaultedRead = 11ULL << 32 | kIsDestructive | kIsNonBlocking |
		kIsDefaulted | kIsConsumer,
	kTryOpen = 12ULL << 32 | kIsDestructive | kIsNonBlocking | kIsConsumer,
	kOpen = 13ULL << 32 | kIsDestructive | kIsBlocking | kIsConsumer,
	kTestFull = 14ULL << 32 | kIsProducer,
	kTryWrite = 15ULL << 32 | kIsDestructive | kIsNonBlocking | kIsProducer,
	kWrite = 16ULL << 32 | kIsDestructive | kIsBlocking | kIsProducer,
	kTryClose = 17ULL << 32 | kIsDestructive | kIsNonBlocking | kIsProducer,
	kClose = 18ULL << 32 | kIsDestructive | kIsBlocking | kIsProducer
};

struct ObjectInfo {
	ObjectInfo(const std::string& name, const std::string& type)
		: name(name), type(type) {
			if (this->type.substr(0, 7) == "struct ") {
				this->type.erase(0, 7);
			} else if (this->type.substr(0, 6) == "class ") {
				this->type.erase(0, 6);
			}
		}
	std::string name;
	std::string type;
};

struct StreamInfo : public ObjectInfo {
	StreamInfo(const std::string& name, const std::string& type, bool is_producer,
			bool is_consumer)
		: ObjectInfo(name, type),
		is_producer(is_producer),
		is_consumer(is_consumer) {}
	// List of operations performed.
	std::vector<StreamOpEnum> ops{};
	// List of AST nodes. Each of them corresponds to a stream operation.
	std::vector<const clang::CXXMemberCallExpr*> call_exprs{};
	bool is_producer{false};
	bool is_consumer{false};
	bool is_blocking{false};
	bool is_non_blocking{false};
	bool need_peeking{false};
	bool need_eos{false};

	std::string PeekVar() const { return name + ".peek_val"; }
	std::string FifoVar() const { return name + ".fifo"; }
	static std::string ProceedVar() { return "tapa_proceed"; }
};

bool IsStmtLoop(const Stmt* stmt){
	auto StmtClass = stmt->getStmtClass();
	if( StmtClass == clang::Stmt::DoStmtClass || StmtClass == clang::Stmt::ForStmtClass || StmtClass == clang::Stmt::WhileStmtClass ){
		return true;
	}
	else{
		return false;
	}
}

void GetRefList(const clang::Stmt* root, const clang::NamedDecl* decl, std::vector<const clang::DeclRefExpr*> & ref_list){
        if ( auto expr = dyn_cast<clang::DeclRefExpr>(root)) {
                const clang::NamedDecl* root_decl = expr->getFoundDecl();
                if( decl == nullptr ){
                        ref_list.push_back(expr);
                }
                else if( decl == root_decl ){
                        ref_list.push_back(expr);
                }
        }

        for (auto stmt : root->children()) {
                if (stmt != nullptr) {
                        GetRefList(stmt, decl, ref_list);
                }
        }
}

void AddNonDuplicate(const clang::NamedDecl* new_decl, std::vector<const clang::NamedDecl*> & list){
	string new_decl_name = new_decl->getNameAsString();

	bool found = false;
	for(auto old_decl : list){
		if( old_decl == new_decl ){
#ifdef DEBUG
			cout << "duplicate " << new_decl_name << " ignored." << endl;
#endif
			found = true;
			break;
		}	
	}

	if( found == false ){
		list.push_back(new_decl);
#ifdef DEBUG
		cout << "Decl " << new_decl_name << " added." << endl;
#endif
	}

}

void GetLoopIterList(const clang::Stmt* root, std::vector<const clang::NamedDecl*> & iter_list){
	/*
	if( auto oper = dyn_cast<clang::CompoundAssignOperator>(root) ){
		const clang::Expr* lhs = oper->getLHS();

	        if ( auto declrefexpr = dyn_cast<clang::DeclRefExpr>(lhs)) {
        	        auto root_decl = declrefexpr->getFoundDecl();
                	if( root_decl != nullptr ){
				auto decl_name = root_decl->getNameAsString();
				//cout << "lhs:" << name << endl;
				if( decl_name.find("operator") == string::npos ){
					AddNonDuplicate(root_decl, iter_list);
				}
                        }
		}
	}
	*/
	if( auto oper = dyn_cast<clang::UnaryOperator>(root) ){
		if( oper->isIncrementOp() || oper->isDecrementOp() ){
			const clang::Expr* subexpr = oper->getSubExpr();
			if ( auto declrefexpr = dyn_cast<clang::DeclRefExpr>(subexpr)) {
				auto root_decl = declrefexpr->getFoundDecl();
				if( root_decl != nullptr ){
					auto decl_name = root_decl->getNameAsString();
					//cout << "lhs:" << name << endl;
					if( decl_name.find("operator") == string::npos ){
						AddNonDuplicate(root_decl, iter_list);
					}
				}
			}
		}
	}
	else if( auto oper = dyn_cast<clang::BinaryOperator>(root) ){
		if( oper->isAssignmentOp() || oper->isCompoundAssignmentOp() || oper->isShiftAssignOp() ){
			const clang::Expr* lhs = oper->getLHS();
			if ( auto declrefexpr = dyn_cast<clang::DeclRefExpr>(lhs)) {
				auto root_decl = declrefexpr->getFoundDecl();
				if( root_decl != nullptr ){
					auto decl_name = root_decl->getNameAsString();
					//cout << "lhs:" << name << endl;
					if( decl_name.find("operator") == string::npos ){
						AddNonDuplicate(root_decl, iter_list);
					}
				}
			}
		}
	}

        for (auto child_stmt : root->children()) {
                if (child_stmt != nullptr) {
                        GetLoopIterList(child_stmt, iter_list);
                }
        }
}

void GetLoopIterList(const clang::Expr* root, std::vector<const clang::NamedDecl*> & iter_list){
        if ( auto root_stmt = dyn_cast<clang::Stmt>(root)) {
                GetLoopIterList(root_stmt, iter_list);
        }
}

void GetDeclRefList(const clang::Stmt* root, std::vector<const clang::NamedDecl*> & decl_list){
        if ( auto expr = dyn_cast<clang::DeclRefExpr>(root)) {
                auto root_decl = expr->getFoundDecl();
                if( root_decl != nullptr ){
                        string decl_name = root_decl->getNameAsString();
                        //cout << "found named decl : " << decl_name << endl;
                        if( decl_name.find("operator") == string::npos ){
				AddNonDuplicate(root_decl, decl_list);
                        }
                }
        }

        for (auto child_stmt : root->children()) {
                if (child_stmt != nullptr) {
                        GetDeclRefList(child_stmt, decl_list);
                }
        }
}

void GetDeclRefList(const clang::Expr* root, std::vector<const clang::NamedDecl*> & decl_list){
        if ( auto root_stmt = dyn_cast<clang::Stmt>(root)) {
                GetDeclRefList(root_stmt, decl_list);
        }
}

bool FindBreakContinueStmt(const clang::Stmt* root)
{
        bool has_break_continue = false;;

        if ( auto break_stmt = dyn_cast<clang::BreakStmt>(root)) {
                has_break_continue = true;
        }
        else if ( auto continue_stmt = dyn_cast<clang::ContinueStmt>(root)) {
                has_break_continue = true;
        }

        if( has_break_continue == false ){
                for (auto child : root->children()) {
                        if (child != nullptr) {
                                bool temp = FindBreakContinueStmt(child);
                                if( temp == true ){
                                        has_break_continue = true;
                                        break;
                                }
                        }
                }
        }
        return has_break_continue;
}

const clang::Stmt* GetLoopInit(const clang::Stmt* loop) {
	if (loop != nullptr) {
		if (auto stmt = llvm::dyn_cast<clang::ForStmt>(loop)) {
			return stmt->getInit();
		}
		if (auto stmt = llvm::dyn_cast<clang::CXXForRangeStmt>(loop)) {
			return stmt->getInit();
		}
	}
	return nullptr;
}

const clang::Expr* GetLoopCond(const clang::Stmt* loop) {
	if (loop != nullptr) {
		if (auto stmt = llvm::dyn_cast<clang::DoStmt>(loop)) {
			return stmt->getCond();
		}
		if (auto stmt = llvm::dyn_cast<clang::ForStmt>(loop)) {
			return stmt->getCond();
		}
		if (auto stmt = llvm::dyn_cast<clang::WhileStmt>(loop)) {
			return stmt->getCond();
		}
		if (auto stmt = llvm::dyn_cast<clang::CXXForRangeStmt>(loop)) {
			return stmt->getCond();
		}
	}
	return nullptr;
}


const clang::Expr* GetLoopInc(const clang::Stmt* loop) {
	if (loop != nullptr) {
		if (auto stmt = llvm::dyn_cast<clang::ForStmt>(loop)) {
			return stmt->getInc();
		}
		if (auto stmt = llvm::dyn_cast<clang::CXXForRangeStmt>(loop)) {
			return stmt->getInc();
		}
	}
	return nullptr;
}

template <typename T>
inline std::string GetNameOfFirst(
		const clang::Stmt* stmt,
		std::shared_ptr<std::unordered_map<const clang::Stmt*, bool>> visited = nullptr) {
	if (visited == nullptr) {
		visited = std::make_shared<decltype(visited)::element_type>();
	}
	if ((*visited)[stmt]) {
		// If stmt is visited for the second, it must have returned empty string
		// last time.
		return "";
	}
	(*visited)[stmt] = true;

	if (const auto expr = llvm::dyn_cast<T>(stmt)) {
		return expr->getNameInfo().getAsString();
	}
	for (const auto child : stmt->children()) {
		if (stmt == nullptr) {
			continue;
		}
		const auto child_result = GetNameOfFirst<T>(child, visited);
		if (!child_result.empty()) {
			return child_result;
		}
	}
	return "";
}

StreamOpEnum GetStreamOp(const clang::CXXMemberCallExpr* call_expr) {
	// Check that the caller has type tapa::stream.
	if (!IsStreamInterface(call_expr->getRecordDecl())) {
		return StreamOpEnum::kNotStreamOperation;
	}
	// Check caller name and number of arguments.
	const string callee = call_expr->getMethodDecl()->getNameAsString();
	const auto num_args = call_expr->getNumArgs();
#ifdef DEBUG 
	cout << "GetStreamOp callee: " << callee << " num_args : " << num_args << endl;
#endif
	if (callee == "empty" && num_args == 0) {
		return StreamOpEnum::kTestEmpty;
	} else if (callee == "try_eos" && num_args == 1) {
		return StreamOpEnum::kTryEos;
	} else if (callee == "eos") {
		if (num_args == 0) {
			return StreamOpEnum::kBlockingEos;
		} else if (num_args == 1) {
			return StreamOpEnum::kNonBlockingEos;
		}
	} else if (callee == "try_peek" && num_args == 1) {
		return StreamOpEnum::kTryPeek;
	} else if (callee == "peek") {
		if (num_args == 1) {
			return StreamOpEnum::kNonBlockingPeek;
		} else if (num_args == 2) {
			return StreamOpEnum::kNonBlockingPeekWithEos;
		}
	} else if (callee == "try_read" && num_args == 1) {
		return StreamOpEnum::kTryRead;
	} else if (callee == "read") {
		if (num_args == 0) {
			return StreamOpEnum::kBlockingRead;
		} else if (num_args == 1) {
			return StreamOpEnum::kNonBlockingRead;
		} else if (num_args == 2) {
			return StreamOpEnum::kNonBlockingDefaultedRead;
		}
	} else if (callee == "try_open" && num_args == 0) {
		return StreamOpEnum::kTryOpen;
	} else if (callee == "open" && num_args == 0) {
		return StreamOpEnum::kOpen;
	} else if (callee == "full" && num_args == 0) {
		return StreamOpEnum::kTestFull;
	} else if (callee == "write" && num_args == 1) {
		return StreamOpEnum::kWrite;
	} else if (callee == "try_write" && num_args == 1) {
		return StreamOpEnum::kTryWrite;
	} else if (callee == "close" && num_args == 0) {
		return StreamOpEnum::kClose;
	} else if (callee == "try_close" && num_args == 0) {
		return StreamOpEnum::kTryClose;
	}

	return StreamOpEnum::kNotStreamOperation;
}

// Retrive information about the given streams.
void GetStreamInfo(const clang::Stmt* root, std::vector<StreamInfo>& arg_streams) {

	if( auto decl = dyn_cast<const clang::DeclRefExpr>(root) ){
#ifdef DEBUG
		//cout << "GetStreamInfo name: " << decl->getNameInfo().getName().getAsString() << endl;
#endif
	}

	for (auto stmt : root->children()) {
		if (stmt == nullptr) {
			continue;
		}
		GetStreamInfo(stmt, arg_streams);

		if (const auto call_expr = dyn_cast<clang::CXXMemberCallExpr>(stmt)) {
			const string callee = call_expr->getMethodDecl()->getNameAsString();
			const string caller = GetNameOfFirst<clang::DeclRefExpr>(call_expr->getImplicitObjectArgument());
			for (auto& stream : arg_streams) {
				if (stream.name == caller) {
#ifdef DEBUG
					//cout << "GetStreamInfo stream.name: " << caller << endl;
#endif
					const StreamOpEnum op = GetStreamOp(call_expr);

					if ( (uint64_t)op & (uint64_t)StreamOpEnum::kIsConsumer ) {
						//cout << "is_consumer" << endl;
						stream.is_consumer = true;
					}
					if ( (uint64_t)op & (uint64_t)StreamOpEnum::kIsProducer ) {
						//cout << "is_producer" << endl;
						stream.is_producer = true;
					}
					if ( (uint64_t)op & (uint64_t)StreamOpEnum::kIsBlocking ) {
						//cout << "is_blocking" << endl;
						stream.is_blocking = true;
					}
					if ( (uint64_t)op & (uint64_t)StreamOpEnum::kIsNonBlocking ) {
						stream.is_non_blocking = true;
					}
					if ( (uint64_t)op & (uint64_t)StreamOpEnum::kNeedPeeking ) {
						stream.need_peeking = true;
					}
					if ( (uint64_t)op & (uint64_t)StreamOpEnum::kNeedEos ) {
						stream.need_eos = true;
					}
					stream.ops.push_back(op);
					stream.call_exprs.push_back(call_expr);

					break;
				}
			}
		}
	}
}

void GetRefStream(const clang::Stmt* loop_body, std::vector<StreamInfo> arg_streams, std::vector<StreamInfo> & ref_stream ) 
{
	if( auto decl = dyn_cast<const clang::DeclRefExpr>(loop_body) ){
#ifdef DEBUG
		//cout << "GetRefStream name: " << decl->getNameInfo().getName().getAsString() << endl;
#endif
	}

	for (auto stmt : loop_body->children()) {
		if (stmt == nullptr) {
			continue;
		}
		GetRefStream(stmt, arg_streams, ref_stream);

		if (const auto call_expr = dyn_cast<clang::CXXMemberCallExpr>(stmt)) {
			const string callee = call_expr->getMethodDecl()->getNameAsString();
			const string caller = GetNameOfFirst<clang::DeclRefExpr>(call_expr->getImplicitObjectArgument());
			for (auto stream : arg_streams) {
				if (stream.name == caller) {
#ifdef DEBUG
					//cout << "GetRefStream stream.name: " << caller << endl;
#endif
					bool exists = false;
					for(int i = 0; i < ref_stream.size(); i++ ){
						if(stream.name == ref_stream[i].name){
							exists = true;
							break;
						}
					}
					if( exists == false ){
						ref_stream.push_back(stream);
					}

					break;
				}
			}
		}
	}
}


void CheckFRDWR(const clang::Stmt* cur_stmt, bool & has_FWR, bool & is_FRD_blocked, bool & has_break_continue, bool & is_Pipe1Loop, std::vector<StreamInfo> arg_streams){
	if( auto decl = dyn_cast<const clang::DeclRefExpr>(cur_stmt) ){
#ifdef DEBUG
		//cout << "CheckFRDWR name: " << decl->getNameInfo().getName().getAsString() << endl;
#endif
	}

	if (const auto call_expr = dyn_cast<clang::CXXMemberCallExpr>(cur_stmt)) {
		const string caller = GetNameOfFirst<clang::DeclRefExpr>(call_expr->getImplicitObjectArgument());
		for (auto stream : arg_streams) {
			if (stream.name == caller) {
#ifdef DEBUG
				//cout << "CheckFRDWR stream.name: " << caller << endl;
#endif
				if( stream.is_producer == true ){
					has_FWR = true;
				}
				if( stream.is_consumer == true && stream.is_blocking == true ){
					is_FRD_blocked = true;
				}
			}
		}
	}
	else if ( auto break_stmt = dyn_cast<clang::BreakStmt>(cur_stmt)) {
                has_break_continue = true;
        }
        else if ( auto continue_stmt = dyn_cast<clang::ContinueStmt>(cur_stmt)) {
                has_break_continue = true;
        }
	else{
		const clang::Stmt* adj_stmt = cur_stmt;

		if( const auto attr_stmt = dyn_cast<clang::AttributedStmt>(cur_stmt) ){
			auto sub_stmt = attr_stmt->getSubStmt();
			if( IsStmtLoop(sub_stmt) ){
#ifdef DEBUG
				//cout << "Loop attr stmt found" << endl;
#endif
				auto attr_list = attr_stmt->getAttrs();

				for (const auto* attr : attr_list) {
					if (auto pipeline = llvm::dyn_cast<clang::TapaPipelineAttr>(attr)) {
#ifdef DEBUG
						//cout << "TAPA PIPELINE attribute found with II " << pipeline->getII() << endl;
#endif
						if( pipeline->getII() == 1 ){
							is_Pipe1Loop = true;
							adj_stmt = sub_stmt;
							break;
						}
					}
				}
			}

		}

		bool known_FRD_blocked = false;

		for (auto child_stmt : adj_stmt->children()) {
			if (child_stmt == nullptr) {
				continue;
			}

			bool child_has_FWR = false;
			bool child_is_FRD_blocked = false;
			bool child_has_break_continue = false;
			bool child_is_Pipe1Loop = false;

			CheckFRDWR(child_stmt, child_has_FWR, child_is_FRD_blocked, child_has_break_continue, child_is_Pipe1Loop, arg_streams);

			if( is_Pipe1Loop == true ){
				if( child_has_FWR == true ){ has_FWR = true; }
				if( child_is_FRD_blocked == true && child_has_break_continue == false ){ is_FRD_blocked = true; }
			}
			else{
				if( child_has_FWR == true ){ has_FWR = true;}
				if( known_FRD_blocked == false ){
					if( child_is_FRD_blocked == true ){ 
						is_FRD_blocked = true;
						known_FRD_blocked = true;
					}
					else if( child_has_FWR == true || child_has_break_continue == true ){
						is_FRD_blocked = false;
						known_FRD_blocked = true;
					}
				}
			}
		}
	}
}

bool ApplyOpt( clang::Rewriter & rewriter_base, clang::Rewriter & rewriter_opt, const clang::Stmt* loop_stmt, std::vector<StreamInfo> arg_streams, bool is_Pipe1Loop){

	auto loop_body = tapa::internal::GetLoopBody(loop_stmt);
	auto cond_expr = GetLoopCond(loop_stmt);
	auto init_stmt = GetLoopInit(loop_stmt);
	auto inc_expr = GetLoopInc(loop_stmt);


	std::vector<const clang::NamedDecl*> loop_iter_list;
	GetLoopIterList( inc_expr, loop_iter_list );

#ifdef DEBUG
	cout << "Found " << loop_iter_list.size() << " iterators in the loop." << endl;
#endif

	std::vector<const clang::DeclRefExpr*> ref_list;

	for(auto loop_iter : loop_iter_list){
#ifdef DEBUG
		//cout << "Looking for loop iterator " << loop_iter->getNameAsString() << endl;
#endif
		GetRefList( loop_body, loop_iter, ref_list );
#ifdef DEBUG
		//cout << "ref_list size : " << ref_list.size() << endl;
#endif
	}

	if(ref_list.size() != 0 ){
#ifdef DEBUG
		cout << "Skipping loop with " << ref_list.size() << " iterator references in body." << endl;
#endif
		return false;
	}

	const clang::Stmt* opt_child_stmt;
	int valid_child_cnt = 0;
	for (auto child_stmt : loop_body->children()) {
		if (child_stmt != nullptr) {
			valid_child_cnt++;
			opt_child_stmt = child_stmt;
		}
	}

	bool child_optimized = false;
	if( valid_child_cnt == 1 ){
		bool child_has_FWR = false;
		bool child_is_FRD_blocked = false;
		bool child_has_break_continue = false;
		bool child_is_Pipe1Loop = false;

		CheckFRDWR(opt_child_stmt, child_has_FWR, child_is_FRD_blocked, child_has_break_continue, child_is_Pipe1Loop, arg_streams);

		if( child_is_FRD_blocked == true ){
			if( IsStmtLoop(opt_child_stmt) == true ){
				child_optimized = ApplyOpt( rewriter_base, rewriter_opt, opt_child_stmt, arg_streams, child_is_Pipe1Loop );
#ifdef DEBUG
				cout << "Optimizing child loop" << endl;
#endif
			}
			else if( const auto attr_stmt = dyn_cast<clang::AttributedStmt>(opt_child_stmt) ){
				auto sub_stmt = attr_stmt->getSubStmt();
				if( IsStmtLoop(sub_stmt) ){
					child_optimized = ApplyOpt( rewriter_base, rewriter_opt, sub_stmt, arg_streams, child_is_Pipe1Loop );
				}
			}
		}
	}

	
#ifdef DEBUG
	cout << "Optimizing loop with";
	if( child_optimized == false ){
		cout << "out";
	}
	cout << " child optimized" << endl;
#endif

	clang::SourceRange OptLoopHead;
	OptLoopHead.clang::SourceRange::setBegin(loop_stmt->getBeginLoc());
	OptLoopHead.clang::SourceRange::setEnd(loop_body->getBeginLoc());

	if( child_optimized == true ){
		rewriter_opt.ReplaceText(OptLoopHead, "{");
	}
	else{
		rewriter_opt.ReplaceText(OptLoopHead, "for(;;){");
	}

	if( is_Pipe1Loop == true ){
#ifdef DEBUG
		cout << "Optimizing pipelined loop" << endl;
#endif
		string base_loop_head;
		string opt_loop_head;

		std::vector<StreamInfo> ref_stream;
		GetRefStream(loop_body, arg_streams, ref_stream );

		string rd_avail;
		for (const auto stream : ref_stream) {
			if (stream.is_consumer && stream.is_blocking) {
				if (!rd_avail.empty()) {
					rd_avail += " && ";
				}
				rd_avail += "!" + stream.name + ".empty()";
			}
		}
#ifdef DEBUG
		cout << "read available condition : " << rd_avail << endl;
#endif
		if( !rd_avail.empty() ){

			base_loop_head += "for(";

			if( init_stmt != nullptr ){
				string init_str = rewriter_base.getRewrittenText(init_stmt->getSourceRange());
				base_loop_head += init_str;
#ifdef DEBUG
				cout << "init: " << init_str << endl;
#endif
			}
			else{
				base_loop_head += ";";
			}

			string cond_str;
			if( cond_expr != nullptr ){
				string cond_str = rewriter_base.getRewrittenText(cond_expr->getSourceRange());
				base_loop_head += cond_str;
#ifdef DEBUG
				cout << "cond: " << cond_str << endl;
#endif
			}
			base_loop_head += ";){";

			if( inc_expr != nullptr ){
				string inc_str = rewriter_base.getRewrittenText(inc_expr->getSourceRange());
				inc_str += ";\n";
				rewriter_base.InsertText(loop_body->getEndLoc(), inc_str);
#ifdef DEBUG
				cout << "inc: " << inc_str << endl;
#endif
			}

			string if_rd_avail = "\nif(" + rd_avail + "){";
			base_loop_head += if_rd_avail;
			opt_loop_head += if_rd_avail;
			rewriter_base.InsertText(loop_body->getEndLoc(), "}\n");
			rewriter_opt.InsertText(loop_body->getEndLoc(), "}\n");


			clang::SourceRange LoopHead;
			LoopHead.clang::SourceRange::setBegin(loop_stmt->getBeginLoc());
			LoopHead.clang::SourceRange::setEnd(loop_body->getBeginLoc());
			rewriter_base.ReplaceText(LoopHead, base_loop_head);

			rewriter_opt.InsertTextAfterToken(loop_body->getBeginLoc(), opt_loop_head);
		}
	}

	return true;
}


string base_filename;
string opt_filename; 


namespace tapa {
namespace internal {

const string* top_name;

class Consumer : public ASTConsumer {
	public:
		explicit Consumer(ASTContext& context, vector<const FunctionDecl*>& funcs)
			: visitor_{context, funcs, rewriters_, metadata_}, funcs_{funcs} {}
		void HandleTranslationUnit(ASTContext& context) override {
			// First pass traversal extracts all global functions as potential tasks.
			// this->funcs_ stores all potential tasks.
			visitor_.TraverseDecl(context.getTranslationUnitDecl());

			// Look for the top-level task. Starting from there, find all tasks using
			// DFS.
			// Note that task functions cannot share the same name.
			auto& diagnostics_engine = context.getDiagnostics();
			if (top_name == nullptr) {
				static const auto diagnostic_id = diagnostics_engine.getCustomDiagID(
						clang::DiagnosticsEngine::Fatal, "top not set");
				diagnostics_engine.Report(diagnostic_id);
			}

			unordered_map<string, vector<const FunctionDecl*>> func_table;
			for (auto func : funcs_) {
				auto func_name = func->getNameAsString();
				// If a key exists, its corresponding value won't be empty.
#ifdef DEBUG
				cout << "Found func : " << func_name << endl;
#endif
				func_table[func_name].push_back(func);
			}

			auto rewriter_base = Rewriter(context.getSourceManager(), context.getLangOpts());
			auto rewriter_opt = Rewriter(context.getSourceManager(), context.getLangOpts());

			auto tasks = FindAllTasks(func_table[*top_name][0]);
			if( tasks.size() == 0 ){
				cout << "Error. No task found for top_name " << *top_name << ". Exiting." << endl; 
				exit(1);
			}


			funcs_.clear();
			for (auto task : tasks) {
				auto task_name = task->getNameAsString();
#ifdef DEBUG
				cout << "Found task : " << task_name << endl;
#endif
				funcs_.push_back(func_table[task_name][0]);
			}

			vector<string> mem_task_list;

			for (auto task : funcs_) {
				const auto func_body = task->getBody();

				auto task_name = task->getNameAsString();
#ifdef DEBUG
				cout << "Processing task : " << task_name << endl;
#endif

				vector<StreamInfo> arg_streams;

				bool has_Mmap = false;

				for (const auto param : task->parameters()) {
					if (IsStreamInterface(param)) {
						auto elem_type = GetStreamElemType(param);
						arg_streams.emplace_back(param->getNameAsString(),
								elem_type, /*is_producer= */
								IsTapaType(param, "ostream"),
								/* is_consumer= */
								IsTapaType(param, "istream"));
					}
					else if ( param != nullptr ){
						clang::RecordDecl* decl = param->getType()->getAsRecordDecl();
						if( decl != nullptr ){
							if( decl->getQualifiedNameAsString() == "tapa::mmap" ||
									decl->getQualifiedNameAsString() == "tapa::async_mmap"){
								has_Mmap = true;
							}
						}
					}
				}

				if( has_Mmap == true ){
#ifdef DEBUG
					cout << "Skipping task : has external memory access" << endl;
#endif
					mem_task_list.push_back(task_name);
					continue;
				}

				GetStreamInfo(func_body, arg_streams);

				auto size = std::distance(func_body->children().begin(), func_body->children().end());
				for( int i = 0 ; i < size; i++ ){
					auto child_iter = func_body->children().begin();
					for( int j = 0; j < size-1-i; j++ ){ child_iter++; }

				//for (auto child_iter = func_body->children().rbegin(); child_iter != func_body->children().rend(); child_iter++ ){
					if (*child_iter == nullptr) {
						continue;
					}

					bool child_has_FWR = false;
					bool child_is_FRD_blocked = false;
					bool child_has_break_continue = false;
					bool child_is_Pipe1Loop = false;

					CheckFRDWR( *child_iter, child_has_FWR, child_is_FRD_blocked, child_has_break_continue, child_is_Pipe1Loop, arg_streams);

					if( child_is_FRD_blocked == false ){
						if( child_has_FWR == true ){
#ifdef DEBUG
							cout << "Write access at end. Cannot transform loops in the task." << endl;
#endif
							break;
						}
						else{
							//go to next statement
						}
					}
					else{
						if( IsStmtLoop(*child_iter) == true ){
							ApplyOpt( rewriter_base, rewriter_opt, *child_iter, arg_streams, child_is_Pipe1Loop );
						}
						else if( const auto attr_stmt = dyn_cast<clang::AttributedStmt>(*child_iter) ){
							auto sub_stmt = attr_stmt->getSubStmt();
							if( IsStmtLoop(sub_stmt) ){
								ApplyOpt( rewriter_base, rewriter_opt, sub_stmt, arg_streams, child_is_Pipe1Loop );
							}
						}
						else{
#ifdef DEBUG
							cout << "Read access at end. Cannot transform loops in the task." << endl;
#endif
						}
						break;
					}
				}
			}


			auto top_func_body = func_table[*top_name][0]->getBody();

			vector<const clang::CXXMemberCallExpr*> invokes = GetTapaInvokes(top_func_body);

			for (auto invoke : invokes) {
				if (const auto method = dyn_cast<clang::CXXMethodDecl>(invoke->getCalleeDecl())) {
					//auto args = method->getTemplateSpecializationArgs()->asArray();
					const auto arg = invoke->getArg(0);
					const auto decl_ref = dyn_cast<clang::DeclRefExpr>(arg);
					if (decl_ref) {
						string arg_name = decl_ref->getNameInfo().getName().getAsString();
						//cout << "Invoke name: " << arg_name << endl;
						bool isMemTask = false;
						for(auto task_name : mem_task_list){
							if( task_name == arg_name ){
#ifdef DEBUG
								//cout << "Optimized task invoked: " << arg_name << endl;
								cout << "Skipping mem access task invoked: " << arg_name << endl;
#endif
								isMemTask = true;
								break;
							}
						}
						if( isMemTask == false ){
							rewriter_opt.InsertTextAfterToken(invoke->getCallee()->getEndLoc(), "<detach>");
						}
					}
				}
			}

			std::string base_string, opt_string;
			raw_string_ostream base_oss(base_string);
			raw_string_ostream opt_oss(opt_string);
			rewriter_base.getEditBuffer(rewriter_base.getSourceMgr().getMainFileID()).write(base_oss);
			rewriter_opt.getEditBuffer(rewriter_opt.getSourceMgr().getMainFileID()).write(opt_oss);
			base_oss.flush();
			opt_oss.flush();

			opt_string = "#include <tapa.h>\nusing tapa::detach;\nusing tapa::join;\n\n" + opt_string;
#ifdef DEBUG
			cout << base_string;
			cout << opt_string;
#endif
			std::ofstream fbase;
			fbase.open(base_filename);
			fbase << base_string;
			fbase.close();

			std::ofstream fopt;
			fopt.open(opt_filename);
			fopt << opt_string;
			fopt.close();
		}

	private:
		Visitor visitor_;
		vector<const FunctionDecl*>& funcs_;
		unordered_map<const FunctionDecl*, Rewriter> rewriters_;
		unordered_map<const FunctionDecl*, json> metadata_;
};


class Action : public ASTFrontendAction {
	public:
		unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& compiler,
				StringRef file) override {
			return llvm::make_unique<Consumer>(compiler.getASTContext(), funcs_);
		}

	private:
		vector<const FunctionDecl*> funcs_;
};

}
}

static OptionCategory tapa_option_category("TAPA AutoRun Optimizer (TARO) Compiler Companion");
static llvm::cl::opt<string> option_top_name( "top", NumOccurrencesFlag::Required, ValueExpected::ValueRequired, llvm::cl::desc("Top-level task name"), llvm::cl::cat(tapa_option_category));
static llvm::cl::opt<string> option_base_filename( "base-file", NumOccurrencesFlag::Required, ValueExpected::ValueRequired, llvm::cl::desc("Baseline filename"), llvm::cl::cat(tapa_option_category));
static llvm::cl::opt<string> option_opt_filename( "opt-file", NumOccurrencesFlag::Required, ValueExpected::ValueRequired, llvm::cl::desc("Optimized filename"), llvm::cl::cat(tapa_option_category));

int main(int argc, const char** argv) {
	CommonOptionsParser parser{argc, argv, tapa_option_category};
	ClangTool tool{parser.getCompilations(), parser.getSourcePathList()};
	string top_name{option_top_name.getValue()};
	tapa::internal::top_name = &top_name;
	base_filename = option_base_filename.getValue();
	opt_filename = option_opt_filename.getValue();
	int ret = tool.run(newFrontendActionFactory<tapa::internal::Action>().get());
	return ret;
}
