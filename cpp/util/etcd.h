#ifndef CERT_TRANS_UTIL_ETCD_H_
#define CERT_TRANS_UTIL_ETCD_H_

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <stdint.h>
#include <string>
#include <vector>

#include "base/macros.h"
#include "net/url_fetcher.h"
#include "util/status.h"
#include "util/task.h"

class JsonObject;

namespace cert_trans {


class EtcdClient {
 public:
  struct Node {
    static const Node& InvalidNode();

    Node() : Node(InvalidNode()) {
    }

    Node(int64_t created_index, int64_t modified_index, const std::string& key,
         bool is_dir, const std::string& value, std::vector<Node>&& nodes,
         bool deleted);

    bool HasExpiry() const;

    std::string ToString() const;

    int64_t created_index_;
    int64_t modified_index_;
    std::string key_;
    bool is_dir_;
    std::string value_;
    std::vector<Node> nodes_;
    std::chrono::system_clock::time_point expires_;
    bool deleted_;
  };

  struct Request {
    Request(const std::string& thekey)
        : key(thekey), recursive(false), wait_index(0) {
    }

    std::string key;
    bool recursive;
    int64_t wait_index;
  };

  struct Response {
    Response() : etcd_index(-1) {
    }

    int64_t etcd_index;
  };

  struct GetResponse : public Response {
    Node node;
  };

  struct GenericResponse : public Response {
    std::shared_ptr<JsonObject> json_body;
  };

  typedef std::function<void(const std::vector<Node>& updates)> WatchCallback;

  // TODO(pphaneuf): This should take a set of servers, not just one.
  EtcdClient(UrlFetcher* fetcher, const std::string& host, uint16_t port);

  virtual ~EtcdClient();

  virtual void Get(const Request& req, GetResponse* resp, util::Task* task);

  virtual void Create(const std::string& key, const std::string& value,
                      Response* resp, util::Task* task);

  virtual void CreateWithTTL(const std::string& key, const std::string& value,
                             const std::chrono::seconds& ttl, Response* resp,
                             util::Task* task);

  virtual void Update(const std::string& key, const std::string& value,
                      const int64_t previous_index, Response* resp,
                      util::Task* task);

  virtual void UpdateWithTTL(const std::string& key, const std::string& value,
                             const std::chrono::seconds& ttl,
                             const int64_t previous_index, Response* resp,
                             util::Task* task);

  virtual void ForceSet(const std::string& key, const std::string& value,
                        Response* resp, util::Task* task);

  virtual void ForceSetWithTTL(const std::string& key,
                               const std::string& value,
                               const std::chrono::seconds& ttl, Response* resp,
                               util::Task* task);

  virtual void Delete(const std::string& key, const int64_t current_index,
                      util::Task* task);

  // The "cb" will be called on the "task" executor. Also, only one
  // will be sent to the executor at a time (for a given call to this
  // method, not for all of them), to make sure they are received in
  // order.
  virtual void Watch(const std::string& key, const WatchCallback& cb,
                     util::Task* task);

 protected:
  // Testing only
  EtcdClient();

 private:
  typedef std::pair<std::string, uint16_t> HostPortPair;

  struct RequestState;
  struct WatchState;

  HostPortPair GetEndpoint() const;
  HostPortPair UpdateEndpoint(const std::string& host, uint16_t port);
  void FetchDone(RequestState* etcd_req, util::Task* task);
  void Generic(const std::string& key,
               const std::map<std::string, std::string>& params,
               UrlFetcher::Verb verb, GenericResponse* resp, util::Task* task);

  void WatchInitialGetDone(WatchState* state, GetResponse* resp,
                           util::Task* task);
  void SendWatchUpdates(WatchState* state, const std::vector<Node>& updates);
  void StartWatchRequest(WatchState* state);
  void WatchRequestDone(WatchState* state, GetResponse* gen_resp,
                        util::Task* child_task);

  UrlFetcher* const fetcher_;

  mutable std::mutex lock_;
  HostPortPair endpoint_;

  DISALLOW_COPY_AND_ASSIGN(EtcdClient);
};


}  // namespace cert_trans

#endif  // CERT_TRANS_UTIL_ETCD_H_
